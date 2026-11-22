/* Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "hardware_catalog.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <json/json.h>

#include "fs.h"

namespace
{

[[noreturn]] void Fail(const std::string& id, const std::string& what)
{
    throw std::runtime_error("hardware catalog: entry '" + id + "': " + what);
}

const Json::Value& Require(const Json::Value& v, const char* key,
                           const std::string& id)
{
    if (!v.isMember(key)) {
        Fail(id, std::string("missing required field '") + key + "'");
    }

    return v[key];
}

std::string RequireString(const Json::Value& v, const char* key,
                          const std::string& id)
{
    const Json::Value& f = Require(v, key, id);

    if (!f.isString()) {
        Fail(id, std::string("field '") + key + "' must be a string");
    }

    return f.asString();
}

int RequireInt(const Json::Value& v, const char* key, const std::string& id)
{
    const Json::Value& f = Require(v, key, id);

    if (!f.isInt()) {
        Fail(id, std::string("field '") + key + "' must be an integer");
    }

    return f.asInt();
}

bool OptBool(const Json::Value& v, const char* key, bool def)
{
    return v.isMember(key) && v[key].isBool() ? v[key].asBool() : def;
}

/** -1 for a missing or null value. */
int OptIntOrNone(const Json::Value& v, const char* key)
{
    return (v.isMember(key) && v[key].isInt()) ? v[key].asInt() : -1;
}

std::string OptString(const Json::Value& v, const char* key)
{
    return (v.isMember(key) && v[key].isString()) ? v[key].asString()
           : std::string();
}

std::vector<std::string> OptStringList(const Json::Value& v, const char* key,
                                       const std::string& id)
{
    std::vector<std::string> out;

    if (!v.isMember(key)) {
        return out;
    }

    if (!v[key].isArray()) {
        Fail(id, std::string("field '") + key + "' must be an array");
    }

    for (const Json::Value& item : v[key]) {
        if (!item.isString()) {
            Fail(id, std::string("field '") + key
                 + "' must contain strings");
        }

        out.push_back(item.asString());
    }

    return out;
}

const char* const CATEGORY_NAMES[4] = {"probe", "rocket", "manned", "misc"};
const char CATEGORY_LETTERS[4] = {'P', 'R', 'C', 'M'};

int CategoryFromString(const std::string& s)
{
    for (int i = 0; i < 4; i++) {
        if (s == CATEGORY_NAMES[i]) {
            return i;
        }
    }

    return -1;
}

double OptDouble(const Json::Value& v, const char* key, double def)
{
    return (v.isMember(key) && v[key].isNumeric()) ? v[key].asDouble() : def;
}

TechTransferRule ParseRule(const Json::Value& v, const std::string& id)
{
    if (!v.isObject() || !v.isMember("when") || !v["when"].isArray()
        || !v.isMember("safety") || !v["safety"].isInt()) {
        Fail(id, "tech_transfer rule needs 'when' (array) and 'safety'");
    }

    TechTransferRule rule;
    rule.safety = v["safety"].asInt();
    rule.atLeast = v.isMember("at_least") && v["at_least"].isInt()
                   ? v["at_least"].asInt() : 75;

    for (const Json::Value& group : v["when"]) {
        if (!group.isArray() || group.empty()) {
            Fail(id, "tech_transfer 'when' must hold non-empty id arrays");
        }

        std::vector<std::string> ids;

        for (const Json::Value& item : group) {
            if (!item.isString()) {
                Fail(id, "tech_transfer 'when' must contain id strings");
            }

            ids.push_back(item.asString());
        }

        rule.when.push_back(ids);
    }

    if (rule.when.empty()) {
        Fail(id, "tech_transfer rule has an empty 'when'");
    }

    return rule;
}

std::vector<TechTransferRule> ParseRules(const Json::Value& v,
                                         const char* key,
                                         const std::string& id)
{
    std::vector<TechTransferRule> out;

    if (!v.isMember(key)) {
        return out;
    }

    if (!v[key].isArray()) {
        Fail(id, std::string("tech_transfer '") + key + "' must be an array");
    }

    for (const Json::Value& item : v[key]) {
        out.push_back(ParseRule(item, id));
    }

    return out;
}

TechTransfer ParseTechTransfer(const Json::Value& v, const std::string& id)
{
    if (!v.isObject()) {
        Fail(id, "field 'tech_transfer' must be an object");
    }

    TechTransfer tt;
    tt.threshold = ParseRules(v, "threshold", id);

    if (!v.isMember("gradual")) {
        return tt;
    }

    const Json::Value& g = v["gradual"];

    if (!g.isObject() || !g.isMember("base") || !g["base"].isInt()) {
        Fail(id, "tech_transfer 'gradual' needs an integer 'base'");
    }

    tt.hasGradual = true;
    tt.base = g["base"].asInt();
    tt.overrides = ParseRules(g, "overrides", id);

    if (g.isMember("steps")) {
        if (!g["steps"].isArray()) {
            Fail(id, "tech_transfer 'steps' must be an array");
        }

        for (const Json::Value& sv : g["steps"]) {
            TechTransferStep step;
            const std::string op = OptString(sv, "op");

            if (op == "set") {
                step.op = TechTransferStep::Op::Set;
            } else if (op == "max") {
                step.op = TechTransferStep::Op::Max;
            } else if (op == "cap") {
                step.op = TechTransferStep::Op::Cap;
            } else {
                Fail(id, "tech_transfer step: unknown op '" + op + "'");
            }

            if (step.op == TechTransferStep::Op::Cap) {
                if (!sv.isMember("max") || !sv["max"].isNumeric()) {
                    Fail(id, "tech_transfer cap step needs 'max'");
                }

                step.capMax = float(sv["max"].asDouble());
                step.capTo = float(OptDouble(sv, "to", step.capMax));
            } else {
                step.from = RequireString(sv, "from", id);
                step.greaterThan = OptString(sv, "greater_than");
                step.above = RequireInt(sv, "above", id);
                step.offset = RequireInt(sv, "offset", id);
                step.divisor = OptDouble(sv, "divisor", 0.0);

                if (step.divisor <= 0.0) {
                    Fail(id, "tech_transfer step needs a positive 'divisor'");
                }

                step.compareDivisor = OptDouble(sv, "compare_divisor", 0.0);
                step.compareDouble = OptString(sv, "compare_precision")
                                     == "double";
            }

            tt.steps.push_back(step);
        }
    }

    return tt;
}

HardwareEntry ParseEntry(const Json::Value& v)
{
    if (!v.isObject()) {
        throw std::runtime_error("hardware catalog: entry is not an object");
    }

    HardwareEntry e;
    e.id = v.isMember("id") && v["id"].isString() ? v["id"].asString()
           : std::string("<no id>");

    RequireString(v, "id", e.id);
    e.side = RequireString(v, "side", e.id);
    e.player = (e.side == "SV") ? 1 : 0;
    e.category = RequireString(v, "category", e.id);
    e.role = HardwareRoleFromString(RequireString(v, "role", e.id));
    e.name = RequireString(v, "name", e.id);
    e.typeLabel = RequireString(v, "type_label", e.id);
    e.labelReplacesName = OptBool(v, "label_replaces_name", false);
    e.crew = RequireInt(v, "crew", e.id);
    e.unitWeight = RequireInt(v, "unit_weight", e.id);
    e.maxPayload = RequireInt(v, "max_payload", e.id);
    e.durationDays = OptIntOrNone(v, "duration_days");

    std::string level = OptString(v, "duration_level");

    if (!level.empty()) {
        e.durationLevel = level[0];
    }

    e.maxFutureDuration = OptIntOrNone(v, "max_future_duration");
    e.maxPlanDays = OptIntOrNone(v, "max_plan_days");
    e.maxMinEquipment = OptIntOrNone(v, "max_min_equipment");
    e.soyuzLL = OptBool(v, "soyuz_ll", false);

    e.reach = OptString(v, "reach");
    e.canDock = OptBool(v, "can_dock", false);
    e.directAscent = OptBool(v, "direct_ascent", false);
    e.acceptsBoosters = OptBool(v, "accepts_boosters", false);
    e.landerNeedsDockingModule = OptBool(v, "lander_needs_docking_module",
                                         false);
    e.compatibleLanders = OptStringList(v, "compatible_landers", e.id);
    e.compatibleKickers = OptStringList(v, "compatible_kickers", e.id);
    e.kickersWithCapsule = OptStringList(v, "kickers_with_capsule", e.id);
    e.kickersSecondPad = OptStringList(v, "kickers_second_pad", e.id);
    e.probeKicker = OptBool(v, "probe_kicker", false);

    if (v.isMember("tech_transfer")) {
        e.techTransfer = ParseTechTransfer(v["tech_transfer"], e.id);
    }

    e.prestigeFirst = OptString(v, "prestige_first");
    e.animId = RequireString(v, "anim_id", e.id);

    const Json::Value& img = Require(v, "images", e.id);

    if (!img.isObject()) {
        Fail(e.id, "field 'images' must be an object");
    }

    e.images.rd = RequireString(img, "rd", e.id);
    e.images.liftoff = RequireString(img, "liftoff", e.id);
    e.images.vabSprite = OptIntOrNone(img, "vab_sprite");
    e.images.vabSpriteBoosters = OptIntOrNone(img, "vab_sprite_boosters");
    e.images.aprog = OptString(img, "aprog");
    e.images.patchBase = OptIntOrNone(img, "patch_base");
    e.images.mhist = OptIntOrNone(img, "mhist");

    e.helpId = RequireString(v, "help_id", e.id);

    const Json::Value& legacy = Require(v, "legacy", e.id);

    if (!legacy.isObject()) {
        Fail(e.id, "field 'legacy' must be an object");
    }

    e.legacyCat = RequireInt(legacy, "cat", e.id);
    e.legacyIdx = RequireInt(legacy, "idx", e.id);

    return e;
}

bool RoleMatchesCategory(HardwareRole role, int cat)
{
    switch (role) {
    case HardwareRole::OrbitalProbe:
    case HardwareRole::LunarProbe:
    case HardwareRole::InterplanetaryProbe:
        return cat == PROBE_HARDWARE;

    case HardwareRole::Launcher:
    case HardwareRole::Booster:
        return cat == ROCKET_HARDWARE;

    case HardwareRole::Capsule:
    case HardwareRole::Minishuttle:
    case HardwareRole::Lander:
        return cat == MANNED_HARDWARE;

    case HardwareRole::Kicker:
    case HardwareRole::Eva:
    case HardwareRole::DockingModule:
    case HardwareRole::PhotoRecon:
        return cat == MISC_HARDWARE;
    }

    return false;
}

} // namespace


HardwareRole HardwareRoleFromString(const std::string& s)
{
    static const struct {
        const char* name;
        HardwareRole role;
    } table[] = {
        {"orbital_probe", HardwareRole::OrbitalProbe},
        {"lunar_probe", HardwareRole::LunarProbe},
        {"interplanetary_probe", HardwareRole::InterplanetaryProbe},
        {"launcher", HardwareRole::Launcher},
        {"booster", HardwareRole::Booster},
        {"capsule", HardwareRole::Capsule},
        {"minishuttle", HardwareRole::Minishuttle},
        {"lander", HardwareRole::Lander},
        {"kicker", HardwareRole::Kicker},
        {"eva", HardwareRole::Eva},
        {"docking_module", HardwareRole::DockingModule},
        {"photo_recon", HardwareRole::PhotoRecon},
    };

    for (const auto& t : table) {
        if (s == t.name) {
            return t.role;
        }
    }

    throw std::runtime_error("hardware catalog: unknown role '" + s + "'");
}


int PrestigeFirstCategory(const HardwareEntry& entry)
{
    static const struct {
        const char* name;
        int category;
    } table[] = {
        {"OnePerson", Prestige_OnePerson},
        {"TwoPerson", Prestige_TwoPerson},
        {"ThreePerson", Prestige_ThreePerson},
        {"Minishuttle", Prestige_Minishuttle},
        {"FourPerson", Prestige_FourPerson},
    };

    for (const auto& t : table) {
        if (entry.prestigeFirst == t.name) {
            return t.category;
        }
    }

    return -1;
}


MissionHardwareType HardwareEntry::missionType() const
{
    switch (role) {
    case HardwareRole::OrbitalProbe:
    case HardwareRole::LunarProbe:
    case HardwareRole::InterplanetaryProbe:
    case HardwareRole::DockingModule:
        return Mission_Probe_DM;

    case HardwareRole::Launcher:
        return Mission_PrimaryBooster;

    case HardwareRole::Booster:
        return Mission_SecondaryBooster;

    case HardwareRole::Capsule:
    case HardwareRole::Minishuttle:
        return Mission_Capsule;

    case HardwareRole::Lander:
        return Mission_LM;

    case HardwareRole::Kicker:
        return Mission_Kicker;

    case HardwareRole::Eva:
        return Mission_EVA;

    case HardwareRole::PhotoRecon:
        return Mission_PhotoRecon;
    }

    throw std::logic_error("HardwareEntry::missionType: unknown role");
}


HardwareCatalog HardwareCatalog::FromJsonText(const std::string& text)
{
    Json::Value doc;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(text.data(), text.data() + text.size(), &doc,
                       &errors)) {
        throw std::runtime_error("hardware catalog: parse error: " + errors);
    }

    if (!doc.isObject() || !doc.isMember("hardware")
        || !doc["hardware"].isArray()) {
        throw std::runtime_error(
            "hardware catalog: missing 'hardware' array");
    }

    HardwareCatalog catalog;

    for (const Json::Value& v : doc["hardware"]) {
        HardwareEntry e = ParseEntry(v);

        if (catalog.mById.count(e.id)) {
            Fail(e.id, "duplicate id");
        }

        catalog.mById[e.id] = catalog.mEntries.size();
        catalog.mEntries.push_back(e);
    }

    catalog.Validate();
    return catalog;
}


void HardwareCatalog::Validate() const
{
    std::map<std::pair<int, std::pair<int, int>>, std::string> legacySeen;

    for (const HardwareEntry& e : mEntries) {
        if (e.side != "US" && e.side != "SV") {
            Fail(e.id, "side must be 'US' or 'SV'");
        }

        const std::string prefix = e.side + ".";

        if (e.id.size() <= prefix.size()
            || e.id.compare(0, prefix.size(), prefix) != 0) {
            Fail(e.id, "id must start with the side ('" + prefix + "')");
        }

        const int cat = CategoryFromString(e.category);

        if (cat < 0) {
            Fail(e.id, "unknown category '" + e.category + "'");
        }

        if (!RoleMatchesCategory(e.role, cat)) {
            Fail(e.id, "role does not fit category '" + e.category + "'");
        }

        if (e.legacyCat != cat) {
            Fail(e.id, "legacy.cat does not match category");
        }

        if (e.legacyIdx < 0 || e.legacyIdx > 6) {
            Fail(e.id, "legacy.idx out of range");
        }

        auto key = std::make_pair(e.player,
                                  std::make_pair(e.legacyCat, e.legacyIdx));

        if (legacySeen.count(key)) {
            Fail(e.id, "legacy slot already used by " + legacySeen[key]);
        }

        legacySeen[key] = e.id;

        // anim_id: category letter + one digit, e.g. "C2"
        if (e.animId.size() != 2 || e.animId[0] != CATEGORY_LETTERS[cat]
            || e.animId[1] < '0' || e.animId[1] > '9') {
            Fail(e.id, "anim_id must be '" + std::string(1, CATEGORY_LETTERS[cat])
                 + "<digit>', got '" + e.animId + "'");
        }

        if (e.crew < 0 || e.crew > 7) {
            Fail(e.id, "crew out of range 0..7");
        }

        if (e.crew > 0 && e.role != HardwareRole::Capsule
            && e.role != HardwareRole::Minishuttle
            && e.role != HardwareRole::Lander) {
            Fail(e.id, "only capsules, minishuttles and landers have a crew");
        }

        if (e.durationLevel != 0
            && (e.durationLevel < 'A' || e.durationLevel > 'F')) {
            Fail(e.id, "duration_level must be A..F");
        }

        if (!e.prestigeFirst.empty() && PrestigeFirstCategory(e) < 0) {
            Fail(e.id, "unknown prestige_first '" + e.prestigeFirst + "'");
        }

        if (e.helpId.empty() || e.helpId[0] != 'i') {
            Fail(e.id, "help_id must look like 'i<number>'");
        }

        for (const std::string& ref : e.compatibleLanders) {
            auto it = mById.find(ref);

            if (it == mById.end()) {
                Fail(e.id, "compatible_landers: unknown id '" + ref + "'");
            }

            if (mEntries[it->second].role != HardwareRole::Lander) {
                Fail(e.id, "compatible_landers: '" + ref + "' is no lander");
            }

            if (mEntries[it->second].side != e.side) {
                Fail(e.id, "compatible_landers: '" + ref + "' is on the "
                     "other side");
            }
        }

        for (const std::string& ref : e.compatibleKickers) {
            auto it = mById.find(ref);

            if (it == mById.end()) {
                Fail(e.id, "compatible_kickers: unknown id '" + ref + "'");
            }

            if (mEntries[it->second].role != HardwareRole::Kicker) {
                Fail(e.id, "compatible_kickers: '" + ref + "' is no kicker");
            }

            if (mEntries[it->second].side != e.side) {
                Fail(e.id, "compatible_kickers: '" + ref + "' is on the "
                     "other side");
            }
        }

        // Cross references of the new rule fields.
        auto checkRef = [&](const std::string& ref, const char* what,
                            int cat = -1) {
            auto it = mById.find(ref);

            if (it == mById.end()) {
                Fail(e.id, std::string(what) + ": unknown id '" + ref + "'");
            }

            if (mEntries[it->second].side != e.side) {
                Fail(e.id, std::string(what) + ": '" + ref
                     + "' is on the other side");
            }

            if (cat >= 0 && mEntries[it->second].legacyCat != cat) {
                Fail(e.id, std::string(what) + ": '" + ref
                     + "' is of the wrong category");
            }
        };

        for (const std::string& ref : e.kickersWithCapsule) {
            checkRef(ref, "kickers_with_capsule", MISC_HARDWARE);

            if (mEntries[mById.at(ref)].role != HardwareRole::Kicker) {
                Fail(e.id, "kickers_with_capsule: '" + ref + "' is no kicker");
            }
        }

        for (const std::string& ref : e.kickersSecondPad) {
            checkRef(ref, "kickers_second_pad", MISC_HARDWARE);

            if (mEntries[mById.at(ref)].role != HardwareRole::Kicker) {
                Fail(e.id, "kickers_second_pad: '" + ref + "' is no kicker");
            }
        }

        for (const TechTransferStep& st : e.techTransfer.steps) {
            if (st.op != TechTransferStep::Op::Cap) {
                checkRef(st.from, "tech_transfer");

                if (!st.greaterThan.empty()) {
                    checkRef(st.greaterThan, "tech_transfer");
                }
            }
        }

        auto checkRules = [&](const std::vector<TechTransferRule>& rules) {
            for (const TechTransferRule& r : rules) {
                for (const auto& group : r.when) {
                    for (const std::string& ref : group) {
                        checkRef(ref, "tech_transfer");
                    }
                }
            }
        };
        checkRules(e.techTransfer.overrides);
        checkRules(e.techTransfer.threshold);
    }
}


const HardwareEntry* HardwareCatalog::FindById(const std::string& id) const
{
    auto it = mById.find(id);
    return it == mById.end() ? nullptr : &mEntries[it->second];
}


const HardwareEntry* HardwareCatalog::FindByLegacy(int player, int cat,
                                                   int idx) const
{
    for (const HardwareEntry& e : mEntries) {
        if (e.player == player && e.legacyCat == cat && e.legacyIdx == idx) {
            return &e;
        }
    }

    return nullptr;
}


const HardwareEntry* HardwareCatalog::FindByLegacyAnySide(int cat,
                                                          int idx) const
{
    for (const HardwareEntry& e : mEntries) {
        if (e.legacyCat == cat && e.legacyIdx == idx) {
            return &e;
        }
    }

    return nullptr;
}


std::vector<const HardwareEntry*>
HardwareCatalog::FindByRole(HardwareRole role, int player) const
{
    std::vector<const HardwareEntry*> out;

    for (const HardwareEntry& e : mEntries) {
        if (e.role == role && (player < 0 || e.player == player)) {
            out.push_back(&e);
        }
    }

    return out;
}


const HardwareCatalog& GetHardwareCatalog()
{
    static const HardwareCatalog catalog = [] {
        std::string path = locate_file("hardware.json", FT_DATA);

        if (path.empty()) {
            throw std::runtime_error(
                "hardware catalog: unable to locate hardware.json");
        }

        std::ifstream input(path);

        if (!input) {
            throw std::runtime_error(
                "hardware catalog: unable to open " + path);
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        return HardwareCatalog::FromJsonText(buffer.str());
    }();

    return catalog;
}


const HardwareEntry* CatalogEntryForEquipment(const Equipment& program)
{
    int cat = -1;

    for (int i = 0; i < 4; i++) {
        if (program.ID[0] == CATEGORY_LETTERS[i]) {
            cat = i;
        }
    }

    if (cat < 0) {
        return nullptr;
    }

    return GetHardwareCatalog().FindByLegacyAnySide(cat,
            int(program.ID[1] - '0'));
}
