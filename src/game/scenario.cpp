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

#include "scenario.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

#include <json/json.h>

#include "fs.h"
#include "hardware.h"
#include "pace.h"
#include "proto.h"
#include "serialize.h"

namespace
{

const char* const CATEGORY_NAMES[4] = {"probe", "rocket", "manned", "misc"};
const char* const SIDE_NAMES[NUM_PLAYERS] = {"US", "SV"};
const size_t MAX_LIST_SIZE = 100;

[[noreturn]] void Fail(const std::string& id, const std::string& what)
{
    throw std::runtime_error("scenario '" + id + "': " + what);
}

std::string Upper(std::string s)
{
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    return s;
}

std::string Lower(std::string s)
{
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

Json::Value ParseJson(const std::string& text)
{
    Json::Value doc;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(text.data(), text.data() + text.size(), &doc,
                       &errors)) {
        throw std::runtime_error("scenario: parse error: " + errors);
    }

    if (!doc.isObject()) {
        throw std::runtime_error("scenario: document is not an object");
    }

    return doc;
}

// ---------------------------------------------------------------------
// Equipment values addressed by name
// ---------------------------------------------------------------------

struct FieldAccess {
    const char* name;
    int (*get)(const Equipment&);
    void (*set)(Equipment&, int);
};

const FieldAccess FIELDS[] = {
    {"safety",
     [](const Equipment& e) { return int(e.Safety); },
     [](Equipment& e, int v) { e.Safety = v; }},
    {"base",
     [](const Equipment& e) { return int(e.Base); },
     [](Equipment& e, int v) { e.Base = v; }},
    {"init_cost",
     [](const Equipment& e) { return int(e.InitCost); },
     [](Equipment& e, int v) { e.InitCost = v; }},
    {"unit_cost",
     [](const Equipment& e) { return int(e.UnitCost); },
     [](Equipment& e, int v) { e.UnitCost = v; }},
    {"unit_weight",
     [](const Equipment& e) { return int(e.UnitWeight); },
     [](Equipment& e, int v) { e.UnitWeight = v; }},
    {"max_pay",
     [](const Equipment& e) { return int(e.MaxPay); },
     [](Equipment& e, int v) { e.MaxPay = v; }},
    {"rd_cost",
     [](const Equipment& e) { return int(e.RDCost); },
     [](Equipment& e, int v) { e.RDCost = v; }},
    {"max_rd",
     [](const Equipment& e) { return int(e.MaxRD); },
     [](Equipment& e, int v) { e.MaxRD = v; }},
    {"max_safety",
     [](const Equipment& e) { return int(e.MaxSafety); },
     [](Equipment& e, int v) { e.MaxSafety = v; }},
    {"duration",
     [](const Equipment& e) { return int(e.Duration); },
     [](Equipment& e, int v) { e.Duration = v; }},
};

const FieldAccess* FindField(const std::string& name)
{
    for (const FieldAccess& f : FIELDS) {
        if (name == f.name) {
            return &f;
        }
    }

    return nullptr;
}

// ---------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------

int ReadInt(const Json::Value& v, const char* key, const std::string& id,
            const std::string& where)
{
    if (!v.isMember(key) || !v[key].isInt()) {
        Fail(id, where + ": '" + key + "' must be an integer");
    }

    return v[key].asInt();
}

StartStats ParseStats(const Json::Value& v, const std::string& id,
                      const std::string& pid)
{
    if (!v.isObject()) {
        Fail(id, "start_stats '" + pid + "' is not an object");
    }

    const std::string where = "start_stats '" + pid + "'";
    StartStats s;
    s.safety = ReadInt(v, "safety", id, where);
    s.base = v.isMember("base") ? ReadInt(v, "base", id, where) : -1;
    s.initCost = ReadInt(v, "init_cost", id, where);
    s.unitCost = ReadInt(v, "unit_cost", id, where);
    s.unitWeight = ReadInt(v, "unit_weight", id, where);
    s.maxPay = ReadInt(v, "max_pay", id, where);
    s.rdCost = ReadInt(v, "rd_cost", id, where);
    s.maxRD = ReadInt(v, "max_rd", id, where);
    s.maxSafety = ReadInt(v, "max_safety", id, where);
    s.duration = ReadInt(v, "duration", id, where);
    s.num = v.isMember("num") ? ReadInt(v, "num", id, where)
            : int(PROGRAM_NOT_STARTED);

    if (s.safety < 0 || s.safety > 100 || s.maxSafety < 0
        || s.maxSafety > 100 || s.maxRD < 0 || s.maxRD > 100) {
        Fail(id, where + ": safety values must be 0..100");
    }

    return s;
}

RandomExpr ParseExpr(const Json::Value& v, const std::string& id)
{
    RandomExpr e;

    if (v.isNumeric()) {
        e.kind = RandomExpr::Kind::Number;
        e.number = v.asDouble();
        return e;
    }

    if (!v.isObject()) {
        Fail(id, "random: expression must be a number or an object");
    }

    if (v.isMember("max")) {
        e.kind = RandomExpr::Kind::Max;

        if (!v["max"].isArray() || v["max"].size() < 2) {
            Fail(id, "random: 'max' needs an array of two or more values");
        }

        for (const Json::Value& item : v["max"]) {
            e.args.push_back(ParseExpr(item, id));
        }

        return e;
    }

    if (!v.isMember("field") || !v["field"].isString()
        || FindField(v["field"].asString()) == nullptr) {
        Fail(id, "random: expression needs a known 'field'");
    }

    e.field = v["field"].asString();

    if (v.isMember("role")) {
        e.kind = RandomExpr::Kind::Ref;

        if (!v["role"].isString()) {
            Fail(id, "random: 'role' must be a string");
        }

        e.role = HardwareRoleFromString(v["role"].asString());
        e.ordinal = v.isMember("ordinal") && v["ordinal"].isInt()
                    ? v["ordinal"].asInt() : 0;
    } else {
        e.kind = RandomExpr::Kind::Self;
    }

    if (v.isMember("mul")) {
        e.hasMul = true;
        e.mul = v["mul"].asDouble();
    }

    if (v.isMember("div")) {
        e.div = v["div"].asInt();

        if (e.div == 0) {
            Fail(id, "random: 'div' must not be 0");
        }
    }

    if (v.isMember("add")) {
        e.add = v["add"].asInt();
    }

    return e;
}

RandomRule ParseRule(const Json::Value& v, const std::string& id)
{
    if (!v.isObject() || !v.isMember("role") || !v["role"].isString()
        || !v.isMember("ops") || !v["ops"].isArray()) {
        Fail(id, "random: a rule needs 'role' and 'ops'");
    }

    RandomRule rule;
    rule.role = HardwareRoleFromString(v["role"].asString());
    rule.ordinal = v.isMember("ordinal") && v["ordinal"].isInt()
                   ? v["ordinal"].asInt() : 0;

    for (const Json::Value& ov : v["ops"]) {
        RandomOp op;

        if (!ov.isObject() || !ov.isMember("field")
            || !ov["field"].isString()
            || FindField(ov["field"].asString()) == nullptr) {
            Fail(id, "random: an op needs a known 'field'");
        }

        op.field = ov["field"].asString();
        const std::string fn = ov.isMember("fn") && ov["fn"].isString()
                               ? ov["fn"].asString() : std::string();
        size_t nargs = 0;

        if (fn == "number") {
            op.fn = RandomOp::Fn::Number;
            nargs = 2;
        } else if (fn == "min") {
            op.fn = RandomOp::Fn::Min;
            nargs = 1;
        } else if (fn == "const") {
            op.fn = RandomOp::Fn::Const;
            nargs = 1;
        } else {
            Fail(id, "random: unknown fn '" + fn + "'");
        }

        if (!ov.isMember("args") || !ov["args"].isArray()
            || ov["args"].size() != nargs) {
            Fail(id, "random: fn '" + fn + "' needs " + std::to_string(nargs)
                 + " argument(s)");
        }

        for (const Json::Value& a : ov["args"]) {
            op.args.push_back(ParseExpr(a, id));
        }

        if (ov.isMember("base")) {
            op.hasBase = true;
            op.base = ParseExpr(ov["base"], id);
        }

        op.negate = ov.isMember("negate") && ov["negate"].asBool();

        if (ov.isMember("max_value")) {
            op.hasMaxValue = true;
            op.maxValue = ov["max_value"].asInt();
        }

        rule.ops.push_back(op);
    }

    return rule;
}

} // namespace


std::string Scenario::ParentIdOf(const std::string& text)
{
    const Json::Value doc = ParseJson(text);
    return doc.isMember("extends") && doc["extends"].isString()
           ? doc["extends"].asString() : std::string();
}


Scenario Scenario::FromJsonText(const std::string& text,
                                const HardwareCatalog& catalog,
                                const Scenario* parent)
{
    const Json::Value doc = ParseJson(text);
    Scenario s;

    if (!doc.isMember("id") || !doc["id"].isString()
        || doc["id"].asString().empty()) {
        throw std::runtime_error("scenario: missing 'id'");
    }

    const std::string id = doc["id"].asString();
    const bool extends = doc.isMember("extends");

    if (extends && parent == nullptr) {
        Fail(id, "extends a scenario that was not provided");
    }

    if (!extends && parent != nullptr) {
        Fail(id, "has a parent but does not extend one");
    }

    if (parent != nullptr) {
        s = *parent;
    }

    s.mId = id;
    s.mName = doc.isMember("name") && doc["name"].isString()
              ? doc["name"].asString() : id;
    s.mDescription = doc.isMember("description") && doc["description"].isString()
                     ? doc["description"].asString() : std::string();
    s.mHidden = doc.isMember("hidden") && doc["hidden"].isBool()
                ? doc["hidden"].asBool() : false;

    if (!extends || doc.isMember("start_year")) {
        s.mStartYear = ReadInt(doc, "start_year", id, "scenario");
    }

    if (!extends || doc.isMember("end_year")) {
        s.mEndYear = ReadInt(doc, "end_year", id, "scenario");
    }

    if (s.mStartYear < MIN_START_YEAR || s.mStartYear > MAX_START_YEAR) {
        Fail(id, "start_year out of range");
    }

    if (s.mEndYear < s.mStartYear
        || s.mEndYear - s.mStartYear >= MAX_GAME_YEARS) {
        Fail(id, "end_year out of range");
    }

    // Hardware lists.
    auto readLists = [&](const char* key, bool append) {
        if (!doc.isMember(key)) {
            if (!extends && !append) {
                Fail(id, std::string("missing '") + key + "'");
            }

            return;
        }

        const Json::Value& hw = doc[key];

        if (!hw.isObject()) {
            Fail(id, std::string("'") + key + "' must be an object");
        }

        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            if (!hw.isMember(SIDE_NAMES[plr])) {
                if (!append) {
                    Fail(id, std::string("'") + key + "' lacks side "
                         + SIDE_NAMES[plr]);
                }

                continue;
            }

            const Json::Value& side = hw[SIDE_NAMES[plr]];

            for (int c = 0; c < 4; c++) {
                if (!side.isMember(CATEGORY_NAMES[c])) {
                    if (!append) {
                        Fail(id, std::string("'") + key + "' lacks "
                             + SIDE_NAMES[plr] + "." + CATEGORY_NAMES[c]);
                    }

                    continue;
                }

                const Json::Value& arr = side[CATEGORY_NAMES[c]];

                if (!arr.isArray()) {
                    Fail(id, "hardware lists must be arrays");
                }

                if (!append) {
                    s.mLists[plr][c].clear();
                }

                for (const Json::Value& item : arr) {
                    if (!item.isString()) {
                        Fail(id, "hardware lists must hold id strings");
                    }

                    const int idx = catalog.IndexOf(item.asString());

                    if (idx < 0) {
                        Fail(id, "unknown hardware id '" + item.asString()
                             + "'");
                    }

                    s.mLists[plr][c].push_back(idx);
                }
            }
        }
    };

    readLists("hardware", false);
    readLists("hardware_append", true);

    std::set<int> seen;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = 0; c < 4; c++) {
            const std::vector<int>& list = s.mLists[plr][c];

            if (list.size() > MAX_LIST_SIZE) {
                Fail(id, "too many programs in a list");
            }

            for (int idx : list) {
                const HardwareEntry& e = *catalog.At(idx);

                if (e.player != plr) {
                    Fail(id, "'" + e.id + "' is on the wrong side");
                }

                if (e.category != CATEGORY_NAMES[c]) {
                    Fail(id, "'" + e.id + "' is in the wrong category list");
                }

                if (!seen.insert(idx).second) {
                    Fail(id, "'" + e.id + "' is listed twice");
                }
            }
        }
    }

    // Start values.
    if (doc.isMember("start_stats")) {
        const Json::Value& st = doc["start_stats"];

        if (!st.isObject()) {
            Fail(id, "'start_stats' must be an object");
        }

        for (const char* model : {"basic", "historical"}) {
            if (!st.isMember(model)) {
                continue;
            }

            std::map<int, StartStats>& dst =
                (std::string(model) == "basic") ? s.mBasic : s.mHistorical;

            for (const std::string& pid : st[model].getMemberNames()) {
                const int idx = catalog.IndexOf(pid);

                if (idx < 0) {
                    Fail(id, "start_stats: unknown id '" + pid + "'");
                }

                dst[idx] = ParseStats(st[model][pid], id, pid);
            }
        }
    }

    for (int idx : seen) {
        if (s.mBasic.count(idx) == 0) {
            Fail(id, "no basic start values for '" + catalog.At(idx)->id
                 + "'");
        }
    }

    // Random model.
    if (doc.isMember("random")) {
        if (!doc["random"].isArray()) {
            Fail(id, "'random' must be an array");
        }

        s.mRandom.clear();

        for (const Json::Value& rv : doc["random"]) {
            s.mRandom.push_back(ParseRule(rv, id));
        }
    }

    // Every program the random rules address (as target or as reference)
    // must exist on both sides.
    auto roleCount = [&](int plr, HardwareRole role) {
        int n = 0;

        for (int c = 0; c < 4; c++) {
            for (int idx : s.mLists[plr][c]) {
                n += catalog.At(idx)->role == role ? 1 : 0;
            }
        }

        return n;
    };
    std::function<void(const RandomExpr&)> checkExpr;
    checkExpr = [&](const RandomExpr& e) {
        if (e.kind == RandomExpr::Kind::Ref) {
            for (int plr = 0; plr < NUM_PLAYERS; plr++) {
                if (roleCount(plr, e.role) <= e.ordinal) {
                    Fail(id, "random: reference to a missing program");
                }
            }
        }

        for (const RandomExpr& a : e.args) {
            checkExpr(a);
        }
    };

    for (const RandomRule& r : s.mRandom) {
        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            if (roleCount(plr, r.role) <= r.ordinal) {
                Fail(id, "random: rule for a missing program");
            }
        }

        for (const RandomOp& op : r.ops) {
            for (const RandomExpr& a : op.args) {
                checkExpr(a);
            }

            if (op.hasBase) {
                checkExpr(op.base);
            }
        }
    }

    return s;
}


int Scenario::count(int category) const
{
    size_t n = 0;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        n = std::max(n, mLists[plr][category].size());
    }

    return int(n);
}


const StartStats* Scenario::stats(StartModel model, int catalogIndex) const
{
    if (model == StartModel::Historical) {
        auto it = mHistorical.find(catalogIndex);

        if (it != mHistorical.end()) {
            return &it->second;
        }
    }

    auto it = mBasic.find(catalogIndex);
    return it == mBasic.end() ? nullptr : &it->second;
}


const char* DefaultScenarioId()
{
    return "CLASSIC";
}


namespace
{

std::string ReadWholeFile(const std::string& path)
{
    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("scenario: unable to open " + path);
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string ScenarioPath(const std::string& id)
{
    const std::string name = "scenarios/" + Lower(id) + ".json";
    const std::string path = locate_file(name.c_str(), FT_DATA);

    if (path.empty()) {
        throw std::runtime_error("scenario: unknown scenario '" + id + "'");
    }

    return path;
}

} // namespace


const Scenario& GetScenario(const std::string& id)
{
    static std::map<std::string, std::unique_ptr<Scenario>> cache;
    const std::string key = Upper(id);
    auto it = cache.find(key);

    if (it != cache.end()) {
        return *it->second;
    }

    // Reject path tricks; ids are plain names.
    for (char c : key) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            throw std::runtime_error("scenario: invalid scenario id '" + id
                                     + "'");
        }
    }

    const std::string text = ReadWholeFile(ScenarioPath(key));
    const std::string parentId = Scenario::ParentIdOf(text);
    const Scenario* parent = parentId.empty() ? nullptr
                             : &GetScenario(parentId);
    auto scenario = std::make_unique<Scenario>(
                        Scenario::FromJsonText(text, GetHardwareCatalog(),
                                               parent));

    if (Upper(scenario->id()) != key) {
        throw std::runtime_error("scenario: file of '" + key
                                 + "' holds scenario '" + scenario->id()
                                 + "'");
    }

    return *(cache[key] = std::move(scenario));
}


std::vector<std::string> ListScenarios(bool includeHidden)
{
    std::vector<std::string> out;
    const std::string anchor =
        locate_file((std::string("scenarios/")
                     + Lower(DefaultScenarioId()) + ".json").c_str(), FT_DATA);

    if (anchor.empty()) {
        return out;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<std::string> names;

    for (const auto& item : fs::directory_iterator(
                                fs::path(anchor).parent_path(), ec)) {
        if (item.path().extension() == ".json") {
            names.push_back(item.path().stem().string());
        }
    }

    std::sort(names.begin(), names.end());

    for (const std::string& name : names) {
        try {
            const Scenario& s = GetScenario(name);

            if (includeHidden || !s.hidden()) {
                out.push_back(s.id());
            }
        } catch (const std::exception&) {
            // Skip scenarios that fail to load.
        }
    }

    // The default scenario comes first.
    std::stable_partition(out.begin(), out.end(), [](const std::string& n) {
        return n == DefaultScenarioId();
    });
    return out;
}


const Scenario& ScenarioOf(const Players& data)
{
    return GetScenario(data.Scenario.empty() ? std::string(DefaultScenarioId())
                       : data.Scenario);
}


namespace
{

Equipment MakeEquipment(const HardwareCatalog& catalog, int catalogIndex,
                        const StartStats& st)
{
    const HardwareEntry& entry = *catalog.At(catalogIndex);
    Equipment e{};
    e.CatalogIndex = int16_t(catalogIndex);
    strntcpy(e.Name, entry.name.c_str(), sizeof(e.Name));
    e.ID[0] = entry.animId[0];
    e.ID[1] = entry.animId[1];
    e.unused_IDX[0] = e.ID[0];
    e.unused_IDX[1] = e.ID[1];
    e.Safety = st.safety;
    e.Base = st.base >= 0 ? st.base : st.safety;
    e.InitCost = st.initCost;
    e.UnitCost = st.unitCost;
    e.UnitWeight = st.unitWeight;
    e.MaxPay = st.maxPay;
    e.RDCost = st.rdCost;
    e.MaxRD = st.maxRD;
    e.MaxSafety = st.maxSafety;
    e.Duration = st.duration;
    e.Num = st.num;
    return e;
}

std::vector<Equipment>& ListOf(BuzzData& p, int category)
{
    switch (category) {
    case PROBE_HARDWARE:
        return p.Probe;

    case ROCKET_HARDWARE:
        return p.Rocket;

    case MANNED_HARDWARE:
        return p.Manned;

    default:
        return p.Misc;
    }
}

// -- random model ------------------------------------------------------

int RandomAux(int x, int y)
{
    return brandom(y - x + 1) + x;
}

int RandomNumber(int x, int y)
{
    return RandomAux(x / 2, (y + 1) / 2) + RandomAux((x + 1) / 2, y / 2);
}

/**
 * The original "random_min()": MAX(1, random_number(0, n)) with the
 * macro MAX(a, b) = ((a) >= (b) ? (a) : (b)), which evaluates the
 * random number a second time when the first one is above 1.  The second
 * draw is kept, so that the random model reproduces the original
 * sequence for a given seed.
 */
int RandomMin(int n)
{
    if (1 >= RandomNumber(0, n)) {
        return 1;
    }

    return RandomNumber(0, n);
}

/** The n-th program of a player with the given role, nullptr if none. */
Equipment* FindByRole(BuzzData& p, HardwareRole role, int ordinal)
{
    const HardwareCatalog& catalog = GetHardwareCatalog();

    for (int c = 0; c < 4; c++) {
        for (Equipment& e : ListOf(p, c)) {
            const HardwareEntry* entry = catalog.At(e.CatalogIndex);

            if (entry != nullptr && entry->role == role && ordinal-- == 0) {
                return &e;
            }
        }
    }

    return nullptr;
}

double Eval(const RandomExpr& e, BuzzData& p, const Equipment& self)
{
    double v = 0;

    switch (e.kind) {
    case RandomExpr::Kind::Number:
        return e.number;

    case RandomExpr::Kind::Self:
        v = FindField(e.field)->get(self);
        break;

    case RandomExpr::Kind::Ref: {
        const Equipment* ref = FindByRole(p, e.role, e.ordinal);

        if (ref == nullptr) {
            throw std::runtime_error("scenario: random model references a "
                                     "missing program");
        }

        v = FindField(e.field)->get(*ref);
        break;
    }

    case RandomExpr::Kind::Max: {
        int best = int(Eval(e.args.front(), p, self));

        for (const RandomExpr& a : e.args) {
            best = std::max(best, int(Eval(a, p, self)));
        }

        return best;
    }
    }

    if (e.hasMul) {
        v *= e.mul;
    }

    if (e.div != 0) {
        v = int(v) / e.div;
    }

    return v + e.add;
}

void ApplyRule(BuzzData& p, const RandomRule& rule)
{
    Equipment* target = FindByRole(p, rule.role, rule.ordinal);

    if (target == nullptr) {
        return;
    }

    for (const RandomOp& op : rule.ops) {
        auto compute = [&]() {
            int r = 0;

            switch (op.fn) {
            case RandomOp::Fn::Number: {
                const int lo = int(Eval(op.args[0], p, *target));
                const int hi = int(Eval(op.args[1], p, *target));
                r = RandomNumber(lo, hi);
                break;
            }

            case RandomOp::Fn::Min:
                r = RandomMin(int(Eval(op.args[0], p, *target)));
                break;

            case RandomOp::Fn::Const:
                r = int(Eval(op.args[0], p, *target));
                break;
            }

            return (op.hasBase ? int(Eval(op.base, p, *target)) : 0)
                   + (op.negate ? -r : r);
        };

        int value = compute();

        if (op.hasMaxValue) {
            // The original MIN(a, b) macro evaluates "a" again when it is
            // returned, which draws new random numbers; kept.
            value = (value <= op.maxValue) ? compute() : op.maxValue;
        }

        FindField(op.field)->set(*target, value);
    }
}

} // namespace


void ApplyRandomModel(Players& data, const Scenario& scenario)
{
    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (const RandomRule& rule : scenario.randomRules()) {
            ApplyRule(data.P[plr], rule);
        }
    }
}


void BuildHardware(Players& data, const Scenario& scenario, StartModel model)
{
    const HardwareCatalog& catalog = GetHardwareCatalog();
    data.Scenario = scenario.id();
    data.Year = data.StartYear = scenario.startYear();
    data.EndYear = scenario.endYear();

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = 0; c < 4; c++) {
            std::vector<Equipment>& list = ListOf(data.P[plr], c);
            list.clear();
            list.reserve(scenario.list(plr, c).size());

            for (int idx : scenario.list(plr, c)) {
                const StartStats* st = scenario.stats(
                                           model == StartModel::Historical
                                           ? StartModel::Historical
                                           : StartModel::Basic, idx);
                list.push_back(MakeEquipment(catalog, idx, *st));
            }
        }

        data.P[plr].Buy.clear();
        data.P[plr].IntelHardwareTable.clear();
        data.P[plr].Crew.clear();
        data.P[plr].CrewCount.clear();
        SizeHardwareTables(data.P[plr]);
    }

    if (model == StartModel::Random) {
        ApplyRandomModel(data, scenario);
    }
}


void ApplyStartModel(Players& data, const Scenario& scenario,
                     StartModel model)
{
    const HardwareCatalog& catalog = GetHardwareCatalog();

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = 0; c < 4; c++) {
            for (Equipment& e : ListOf(data.P[plr], c)) {
                const StartStats* st = scenario.stats(
                                           model == StartModel::Historical
                                           ? StartModel::Historical
                                           : StartModel::Basic,
                                           e.CatalogIndex);

                if (st != nullptr) {
                    e = MakeEquipment(catalog, e.CatalogIndex, *st);
                }
            }
        }
    }

    if (model == StartModel::Random) {
        ApplyRandomModel(data, scenario);
    }
}


void BuildStartState(Players& data, const std::string& scenarioId,
                     StartModel model)
{
    const std::string path = locate_file("urast.json", FT_DATA);

    if (path.empty()) {
        throw std::runtime_error("start state: unable to locate urast.json");
    }

    DESERIALIZE_JSON_FILE(&data, path);
    BuildHardware(data, GetScenario(scenarioId), model);
}
