/**
 * Tests for the descriptive hardware catalog (data/gamedata/hardware.json).
 *
 * The "golden" cases re-implement the ORIGINAL index-arithmetic formulas
 * (as they were before the catalog was introduced) and require that the
 * catalog yields identical results for every legacy slot.
 */
#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <json/json.h>

#include "game/data.h"
#include "game/fs.h"
#include "game/hardware.h"
#include "game/scenario.h"
#include "game/hardware_catalog.h"
#include "game/mission_util.h"
#include "game/serialize.h"


namespace
{

// ---------------------------------------------------------------------
// Reference implementations: the original code, copied from before the
// catalog was introduced (hardware.cpp, rdplex.cpp, place.cpp).
// ---------------------------------------------------------------------

MissionHardwareType OrigHardwareType(char id0, char id1)
{
    int categoryIndex = int(id1 - '0');

    switch (id0) {
    case 'P':
        return Mission_Probe_DM;

    case 'C':
        return (categoryIndex <= MANNED_HW_FOUR_MAN_CAPSULE)
               ? Mission_Capsule : Mission_LM;

    case 'R':
        return (categoryIndex != ROCKET_HW_BOOSTERS)
               ? Mission_PrimaryBooster : Mission_SecondaryBooster;

    case 'M':
        if (categoryIndex <= MISC_HW_KICKER_C) {
            return Mission_Kicker;
        } else if (categoryIndex == MISC_HW_EVA_SUITS) {
            return Mission_EVA;
        } else if (categoryIndex == MISC_HW_DOCKING_MODULE) {
            return Mission_Probe_DM;
        } else if (categoryIndex == MISC_HW_PHOTO_RECON) {
            return Mission_PhotoRecon;
        }

        break;

    default:
        break;
    }

    throw std::invalid_argument("unrecognized");
}

int OrigCrewSize(char id0, char id1)
{
    MissionHardwareType type = OrigHardwareType(id0, id1);

    if (type != Mission_Capsule && type != Mission_LM) {
        return 0;
    }

    int mannedIndex = int(id1 - '0');

    switch (mannedIndex) {
    case MANNED_HW_ONE_MAN_CAPSULE:
        return 1;

    case MANNED_HW_TWO_MAN_CAPSULE:
        return 2;

    case MANNED_HW_THREE_MAN_CAPSULE:
    case MANNED_HW_MINISHUTTLE:
        return 3;

    case MANNED_HW_FOUR_MAN_CAPSULE:
        return 4;

    case MANNED_HW_TWO_MAN_MODULE:
        return 2;

    case MANNED_HW_ONE_MAN_MODULE:
        return 1;

    default:
        return 0;
    }
}

/** The text ShowUnit() drew after the name, incl. leading space. */
struct OrigLabel {
    std::string suffix;
    bool replacesName = false;
};

OrigLabel OrigTypeLabel(int hw, int un, int player_index)
{
    OrigLabel r;

    switch (hw) {
    case PROBE_HARDWARE:
        switch (un) {
        case PROBE_HW_ORBITAL:
        case PROBE_HW_INTERPLANETARY:
            r.suffix = " SATELLITE";
            break;

        case PROBE_HW_LUNAR:
            r.suffix = " PROBE";
            break;
        }

        break;

    case ROCKET_HARDWARE:
        switch (un) {
        case ROCKET_HW_ONE_STAGE:
        case ROCKET_HW_TWO_STAGE:
        case ROCKET_HW_THREE_STAGE:
        case ROCKET_HW_MEGA_STAGE:
            r.suffix = " ROCKET";
            break;

        case ROCKET_HW_BOOSTERS:
            if (player_index == 0) {
                r.suffix = " STRAP-ON";
            } else {
                r.suffix = "BOOSTER STAGE";  // drawn at the name position
                r.replacesName = true;
            }

            break;
        }

        break;

    case MANNED_HARDWARE:
        switch (un) {
        case MANNED_HW_ONE_MAN_CAPSULE:
        case MANNED_HW_TWO_MAN_CAPSULE:
        case MANNED_HW_THREE_MAN_CAPSULE:
            r.suffix = " CAPSULE";
            break;

        case MANNED_HW_MINISHUTTLE:
            r.suffix = " MINISHUTTLE";
            break;

        case MANNED_HW_FOUR_MAN_CAPSULE:
            r.suffix = " SPACECRAFT";
            break;

        case MANNED_HW_TWO_MAN_MODULE:
        case MANNED_HW_ONE_MAN_MODULE:
            r.suffix = " MODULE";
            break;
        }

        break;

    case MISC_HARDWARE:
        if (un <= MISC_HW_KICKER_C) {
            r.suffix = " BOOSTER";
        }

        if (un == MISC_HW_DOCKING_MODULE) {
            r.suffix = " MODULE";
        }

        break;
    }

    return r;
}

/** Original ShowHardwareDescription() help id. */
std::string OrigHelpId(int player, int hardware, int unit)
{
    int helpIndex = 0;

    switch (hardware) {
    case PROBE_HARDWARE:
        helpIndex = 201;
        break;

    case ROCKET_HARDWARE:
        helpIndex = 207;
        break;

    case MANNED_HARDWARE:
        helpIndex = 217;
        break;

    case MISC_HARDWARE:
        helpIndex = 231;
        break;
    }

    helpIndex = helpIndex + unit * 2 + player;
    char helpEntry[5];
    snprintf(helpEntry, sizeof(helpEntry), "i%d03", helpIndex);
    return helpEntry;
}

/** Original BigHardMe() research image name. */
std::string OrigRdImage(int plr, int hw, int unit)
{
    int index = (plr * 32) + (hw * 8) + unit;
    return "images/rd/rdfull.but." + std::to_string(index) + ".png";
}

/** Original BigHardMe() liftoff image name. */
std::string OrigLiftoffImage(int plr, int hw, int unit)
{
    char name[5];

    if (plr == 0) {
        name[0] = 'U';
        name[1] = 'S';
    } else {
        name[0] = 'S';
        name[1] = 'V';
    }

    switch (hw) {
    case PROBE_HARDWARE:
        name[2] = 'P';
        break;

    case ROCKET_HARDWARE:
        name[2] = 'R';
        break;

    case MANNED_HARDWARE:
        name[2] = 'C';
        break;

    case MISC_HARDWARE:
        name[2] = 'M';
        break;
    }

    name[3] = 0x30 + unit;
    name[4] = '\x00';

    return std::string("images/liftoff.abz.") + name + ".png";
}

const char CATEGORY_ID[4] = {'P', 'R', 'C', 'M'};

/** The classic program (US side, equal to the USSR one for the checked
 *  data) of a category letter and index, identified by its catalog index. */
Equipment MakeEquipment(char category, int index)
{
    Equipment eq{};
    eq.ID[0] = category;
    eq.ID[1] = static_cast<char>('0' + index);

    for (int c = 0; c < 4; c++) {
        if (category == CATEGORY_ID[c]) {
            const HardwareEntry* e =
                GetHardwareCatalog().FindByLegacy(0, c, index);

            if (e != nullptr) {
                eq.CatalogIndex = GetHardwareCatalog().IndexOf(e->id);
            }
        }
    }

    return eq;
}

std::string ReadFile(const std::string& path)
{
    std::ifstream in(path);
    BOOST_REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string CatalogText()
{
    std::string path = locate_file("hardware.json", FT_DATA);
    BOOST_REQUIRE(!path.empty());
    return ReadFile(path);
}

/** Parse the catalog, apply a change and serialize again. */
std::string Mutate(const std::function<void(Json::Value&)>& change)
{
    Json::Value doc;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream in(CatalogText());
    BOOST_REQUIRE(Json::parseFromStream(rb, in, &doc, &errs));
    change(doc);
    Json::StreamWriterBuilder wb;
    return Json::writeString(wb, doc);
}

} // namespace


BOOST_AUTO_TEST_SUITE(hardware_catalog_suite)

// ---------------------------------------------------------------------
// (a) Catalog loads and validates
// ---------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(catalog_loads_and_has_all_legacy_slots)
{
    const HardwareCatalog& cat = GetHardwareCatalog();
    // 21 original programs per side plus the test-only entries
    size_t classic = 0;

    for (const HardwareEntry& e : cat.entries()) {
        classic += e.testOnly ? 0 : 1;
    }

    BOOST_CHECK_EQUAL(classic, 42u);

    for (int player = 0; player < NUM_PLAYERS; player++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            for (int i = 0; i < HardwareCount(c); i++) {
                const HardwareEntry* e = cat.FindByLegacy(player, c, i);
                BOOST_REQUIRE_MESSAGE(e != nullptr,
                                      "no entry for player " << player
                                      << " cat " << c << " idx " << i);
                BOOST_CHECK_EQUAL(e->player, player);
                BOOST_CHECK(cat.FindById(e->id) == e);
            }
        }
    }

    // no slot outside the 21 per side
    BOOST_CHECK(cat.FindByLegacy(0, PROBE_HARDWARE, 3) == nullptr);
    BOOST_CHECK(cat.FindByLegacy(1, MISC_HARDWARE, 6) == nullptr);
}

BOOST_AUTO_TEST_CASE(catalog_lookup_by_id_and_role)
{
    const HardwareCatalog& cat = GetHardwareCatalog();
    const HardwareEntry* atlas = cat.FindById("US.ATLAS");
    BOOST_REQUIRE(atlas);
    BOOST_CHECK_EQUAL(atlas->name, "ATLAS");
    BOOST_CHECK(atlas->role == HardwareRole::Launcher);
    BOOST_CHECK(atlas->acceptsBoosters);

    const HardwareEntry* r7 = cat.FindById("SV.R7");
    BOOST_REQUIRE(r7);
    BOOST_CHECK_EQUAL(r7->legacyIdx, ROCKET_HW_ONE_STAGE);
    BOOST_CHECK(cat.FindById("US.R7") == nullptr);

    BOOST_CHECK_EQUAL(cat.FindByRole(HardwareRole::Launcher).size(), 8u);
    BOOST_CHECK_EQUAL(cat.FindByRole(HardwareRole::Launcher, 0).size(), 4u);
    BOOST_CHECK_EQUAL(cat.FindByRole(HardwareRole::Kicker, 1).size(), 3u);
    BOOST_CHECK_EQUAL(cat.FindByRole(HardwareRole::Lander).size(), 4u);
    BOOST_CHECK_EQUAL(cat.FindByRole(HardwareRole::DockingModule).size(), 2u);
}

BOOST_AUTO_TEST_CASE(catalog_matches_urast_json)
{
    std::unique_ptr<Players> players(new Players());
    BuildStartState(*players, "CLASSIC", StartModel::Basic);

    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            for (int i = 0; i < HardwareCount(c); i++) {
                const Equipment* eq = nullptr;

                switch (c) {
                case PROBE_HARDWARE:
                    eq = &players->P[player].Probe[i];
                    break;

                case ROCKET_HARDWARE:
                    eq = &players->P[player].Rocket[i];
                    break;

                case MANNED_HARDWARE:
                    eq = &players->P[player].Manned[i];
                    break;

                case MISC_HARDWARE:
                    eq = &players->P[player].Misc[i];
                    break;
                }

                const HardwareEntry* e = cat.FindByLegacy(player, c, i);
                BOOST_REQUIRE(e);
                std::string where = e->id;

                BOOST_CHECK_MESSAGE(e->name == eq->Name, where << " name");
                BOOST_CHECK_MESSAGE(e->animId == std::string(eq->ID, 2),
                                    where << " anim_id vs Equipment::ID");
                BOOST_CHECK_MESSAGE(e->durationDays == eq->Duration,
                                    where << " duration");
                // CrewSize on the real Equipment must agree
                BOOST_CHECK_MESSAGE(CrewSize(*eq) == e->crew,
                                    where << " crew");
                BOOST_CHECK_MESSAGE(HardwareType(*eq) == e->missionType(),
                                    where << " mission type");
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(catalog_validation_rejects_bad_input)
{
    // sanity: the unmodified text is accepted
    BOOST_CHECK_NO_THROW(HardwareCatalog::FromJsonText(CatalogText()));

    // duplicate id
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][1]["id"] = d["hardware"][0]["id"];
    })), std::runtime_error);

    // missing required field
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][3].removeMember("help_id");
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][3].removeMember("name");
    })), std::runtime_error);

    // anim_id format
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][3]["anim_id"] = "R";
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][3]["anim_id"] = "X0";
    })), std::runtime_error);

    // invalid references
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.GEMINI") {
                e["compatible_landers"][0] = "US.NOPE";
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.GEMINI") {
                e["compatible_kickers"][0] = "US.ATLAS";  // not a kicker
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.GEMINI") {
                e["compatible_landers"][0] = "SV.LK";  // other side
            }
        }
    })), std::runtime_error);

    // 4b rule data: references and structure
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.TITAN") {
                e["tech_transfer"]["gradual"]["steps"][0]["from"] = "US.NOPE";
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.TITAN") {
                e["tech_transfer"]["threshold"][0]["when"][0][0] = "SV.R7";
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.TITAN") {
                e["tech_transfer"]["gradual"]["steps"][0]["op"] = "mul";
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.MERCURY") {
                e["prestige_first"] = "NoSuchCategory";
            }
        }
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.GEMINI") {
                e["kickers_with_capsule"][0] = "US.ATLAS";  // not a kicker
            }
        }
    })), std::runtime_error);

    // two entries on one legacy slot
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][1]["legacy"]["idx"] = 0;
    })), std::runtime_error);

    // unknown role / side / category
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][0]["role"] = "spaceship";
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][0]["side"] = "XX";
    })), std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        d["hardware"][0]["category"] = "ship";
    })), std::runtime_error);

    // not json
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText("{ nope"),
                      std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText("{}"),
                      std::runtime_error);
}

// ---------------------------------------------------------------------
// (b) Catalog-derived values equal the ORIGINAL formulas, every slot
// ---------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(golden_crew_and_type_equal_original)
{
    for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
        for (int i = 0; i < HardwareCount(c); i++) {
            Equipment eq = MakeEquipment(CATEGORY_ID[c], i);
            BOOST_CHECK_MESSAGE(
                CrewSize(eq) == OrigCrewSize(CATEGORY_ID[c], '0' + i),
                "CrewSize " << CATEGORY_ID[c] << i);
            BOOST_CHECK_MESSAGE(
                HardwareType(eq) == OrigHardwareType(CATEGORY_ID[c], '0' + i),
                "HardwareType " << CATEGORY_ID[c] << i);
        }
    }
}

BOOST_AUTO_TEST_CASE(golden_labels_help_images_equal_original)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            for (int i = 0; i < HardwareCount(c); i++) {
                const HardwareEntry* e = cat.FindByLegacy(player, c, i);
                BOOST_REQUIRE(e);
                std::string where = e->id;

                // type label as ShowUnit() draws it now
                std::string suffix;

                if (e->labelReplacesName) {
                    suffix = e->typeLabel;
                } else if (!e->typeLabel.empty()) {
                    suffix = " " + e->typeLabel;
                }

                OrigLabel o = OrigTypeLabel(c, i, player);
                BOOST_CHECK_MESSAGE(suffix == o.suffix,
                                    where << " type label '" << suffix
                                    << "' vs '" << o.suffix << "'");
                BOOST_CHECK_MESSAGE(e->labelReplacesName == o.replacesName,
                                    where << " label_replaces_name");

                BOOST_CHECK_MESSAGE(e->helpId == OrigHelpId(player, c, i),
                                    where << " help id " << e->helpId);
                BOOST_CHECK_MESSAGE(e->images.rd == OrigRdImage(player, c, i),
                                    where << " rd image " << e->images.rd);
                BOOST_CHECK_MESSAGE(
                    e->images.liftoff == OrigLiftoffImage(player, c, i),
                    where << " liftoff image " << e->images.liftoff);
            }
        }
    }
}

// The other image names computed by index arithmetic in the game:
// PatchMe (patches.but.(50*plr + 10*prog)), SmHardMe (mhist.but.
// (7 + plr*8 + prog)), ast4.cpp (aprog.plr.prog, prog = index + 1),
// vab.cpp (VabSprite enum).
BOOST_AUTO_TEST_CASE(golden_other_image_references)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        for (int i = MANNED_HW_ONE_MAN_CAPSULE; i <= MANNED_HW_FOUR_MAN_CAPSULE;
             i++) {
            const HardwareEntry* e = cat.FindByLegacy(player, MANNED_HARDWARE, i);
            BOOST_REQUIRE(e);
            BOOST_CHECK_EQUAL(e->images.patchBase, 50 * player + 10 * i);
            BOOST_CHECK_EQUAL(e->images.mhist, 7 + player * 8 + i);
            BOOST_CHECK_EQUAL(e->images.aprog,
                              "images/aprog/aprog." + std::to_string(player)
                              + "." + std::to_string(MannedIndexToProgram(i))
                              + ".png");
            // VabImg_OneManCapsule (13) + index; vab.cpp BuildVAB()
            BOOST_CHECK_EQUAL(e->images.vabSprite, 13 + i);
        }

        // Lunar modules use VabImg_TwoManLM (18), VabImg_OneManLM (19)
        BOOST_CHECK_EQUAL(cat.FindByLegacy(player, MANNED_HARDWARE,
                                           MANNED_HW_TWO_MAN_MODULE)
                          ->images.vabSprite, 18);
        BOOST_CHECK_EQUAL(cat.FindByLegacy(player, MANNED_HARDWARE,
                                           MANNED_HW_ONE_MAN_MODULE)
                          ->images.vabSprite, 19);

        // Probes: VabImg_OrbitalSat (9), Interplanetary (10), Lunar (11)
        for (int i = 0; i < PROBE_HW_COUNT; i++) {
            BOOST_CHECK_EQUAL(
                cat.FindByLegacy(player, PROBE_HARDWARE, i)->images.vabSprite,
                9 + i);
        }

        // Rockets: sprite index = rocket index, +4 with boosters
        for (int i = 0; i < ROCKET_HW_BOOSTERS; i++) {
            const HardwareEntry* e = cat.FindByLegacy(player, ROCKET_HARDWARE, i);
            BOOST_CHECK_EQUAL(e->images.vabSprite, i);

            if (i != ROCKET_HW_MEGA_STAGE) {
                BOOST_CHECK_EQUAL(e->images.vabSpriteBoosters, i + 4);
            } else {
                BOOST_CHECK_EQUAL(e->images.vabSpriteBoosters, -1);
            }
        }

        // Kickers: VabImg_KickerA (20) + index; docking module 12
        for (int i = MISC_HW_KICKER_A; i <= MISC_HW_KICKER_C; i++) {
            BOOST_CHECK_EQUAL(
                cat.FindByLegacy(player, MISC_HARDWARE, i)->images.vabSprite,
                20 + i);
        }

        BOOST_CHECK_EQUAL(cat.FindByLegacy(player, MISC_HARDWARE,
                                           MISC_HW_DOCKING_MODULE)
                          ->images.vabSprite, 12);
    }
}

// prest.cpp: Prestige for a manned program is 12 + Prog (Prog = index + 1),
// i.e. categories 13..17.
BOOST_AUTO_TEST_CASE(golden_prestige_first)
{
    const char* names[5] = {"OnePerson", "TwoPerson", "ThreePerson",
                            "Minishuttle", "FourPerson"
                           };
    const int legacy[5] = {Prestige_OnePerson, Prestige_TwoPerson,
                           Prestige_ThreePerson, Prestige_Minishuttle,
                           Prestige_FourPerson
                          };

    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        for (int i = 0; i < 5; i++) {
            const HardwareEntry* e = cat.FindByLegacy(player, MANNED_HARDWARE, i);
            BOOST_CHECK_EQUAL(e->prestigeFirst, names[i]);
            BOOST_CHECK_EQUAL(legacy[i], 12 + MannedIndexToProgram(i));
            // The catalog drives the category (was: Set_Goal(12 + Prog))
            BOOST_CHECK_EQUAL(PrestigeFirstCategory(*e),
                              12 + MannedIndexToProgram(i));
        }

        for (int i = 5; i < 7; i++) {
            BOOST_CHECK(cat.FindByLegacy(player, MANNED_HARDWARE, i)
                        ->prestigeFirst.empty());
            BOOST_CHECK_EQUAL(PrestigeFirstCategory(
                                  *cat.FindByLegacy(player, MANNED_HARDWARE, i)),
                              -1);
        }
    }
}

// Capability flags against the code rules they were derived from.
BOOST_AUTO_TEST_CASE(golden_capability_flags)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        // Mega rocket takes no boosters (vehicle.cpp)
        for (int i = 0; i < ROCKET_HW_BOOSTERS; i++) {
            BOOST_CHECK_EQUAL(
                cat.FindByLegacy(player, ROCKET_HARDWARE, i)->acceptsBoosters,
                i != ROCKET_HW_MEGA_STAGE);
        }

        BOOST_CHECK(!cat.FindByLegacy(player, ROCKET_HARDWARE,
                                      ROCKET_HW_BOOSTERS)->acceptsBoosters);

        // Capsule limits: compare with the rules in CapsuleEligibility()
        // for a mission without lunar/docking/day restrictions.
        int plainMission = -1;

        for (const mStr& m : GetMissionData()) {
            if (m.Lun == 0 && m.Doc == 0 && m.mEq <= 1 && m.Days == 0
                && m.Index != Mission_DirectAscent_LL) {
                plainMission = m.Index;
                break;
            }
        }

        BOOST_REQUIRE(plainMission >= 0);

        for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
            const HardwareEntry* e = cat.FindByLegacy(player, MANNED_HARDWARE, i);

            for (int dur = 0; dur <= 6; dur++) {
                auto ok = CapsuleEligibility(player, plainMission, dur);
                int mf = e->maxFutureDuration;
                BOOST_CHECK_MESSAGE(ok[i] == (mf < 0 || dur <= mf),
                                    e->id << " duration " << dur);
            }
        }

        // LMAdd() pairing: Gemini -> one-man LM (+DM), Apollo -> both LMs,
        // minishuttle -> both LMs (+DM)
        const HardwareEntry* gemini =
            cat.FindByLegacy(player, MANNED_HARDWARE, MANNED_HW_TWO_MAN_CAPSULE);
        const HardwareEntry* apollo =
            cat.FindByLegacy(player, MANNED_HARDWARE, MANNED_HW_THREE_MAN_CAPSULE);
        const HardwareEntry* shuttle =
            cat.FindByLegacy(player, MANNED_HARDWARE, MANNED_HW_MINISHUTTLE);
        const HardwareEntry* lm1 =
            cat.FindByLegacy(player, MANNED_HARDWARE, MANNED_HW_ONE_MAN_MODULE);
        const HardwareEntry* lm2 =
            cat.FindByLegacy(player, MANNED_HARDWARE, MANNED_HW_TWO_MAN_MODULE);

        BOOST_CHECK((gemini->compatibleLanders
                     == std::vector<std::string> {lm1->id}));
        BOOST_CHECK((apollo->compatibleLanders
                     == std::vector<std::string> {lm2->id, lm1->id}));
        BOOST_CHECK((shuttle->compatibleLanders
                     == std::vector<std::string> {lm2->id, lm1->id}));
        BOOST_CHECK(gemini->landerNeedsDockingModule);
        BOOST_CHECK(!apollo->landerNeedsDockingModule);
        BOOST_CHECK(shuttle->landerNeedsDockingModule);

        // Only the Jupiter-class capsule can do direct ascent
        for (int i = 0; i < MANNED_HW_COUNT; i++) {
            BOOST_CHECK_EQUAL(
                cat.FindByLegacy(player, MANNED_HARDWARE, i)->directAscent,
                i == MANNED_HW_FOUR_MAN_CAPSULE);
        }
    }
}

// 4b: the rule fields reproduce the former hard-coded rules
BOOST_AUTO_TEST_CASE(rule_fields_match_former_rules)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (int player = 0; player < NUM_PLAYERS; player++) {
        auto manned = [&](int i) {
            return cat.FindByLegacy(player, MANNED_HARDWARE, i);
        };

        // Eligibility fields (crew.cpp HardRequest/SecondHard)
        BOOST_CHECK_EQUAL(manned(MANNED_HW_ONE_MAN_CAPSULE)->maxMinEquipment, 1);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_ONE_MAN_CAPSULE)->maxPlanDays, 2);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_ONE_MAN_CAPSULE)->maxFutureDuration, 2);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_TWO_MAN_CAPSULE)->maxFutureDuration, 5);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_TWO_MAN_CAPSULE)->maxPlanDays, 5);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_MINISHUTTLE)->maxFutureDuration, 4);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_MINISHUTTLE)->maxPlanDays, -1);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_THREE_MAN_CAPSULE)->maxFutureDuration, -1);
        BOOST_CHECK_EQUAL(manned(MANNED_HW_FOUR_MAN_CAPSULE)->maxFutureDuration, -1);

        // Only the Soviet Soyuz may attempt the Soyuz lunar landing
        for (int i = 0; i < MANNED_HW_COUNT; i++) {
            BOOST_CHECK_EQUAL(manned(i)->soyuzLL,
                              player == 1 && i == MANNED_HW_THREE_MAN_CAPSULE);
        }

        // Kicker C is the probe kicker; the others are not
        for (int i = MISC_HW_KICKER_A; i <= MISC_HW_KICKER_C; i++) {
            BOOST_CHECK_EQUAL(
                cat.FindByLegacy(player, MISC_HARDWARE, i)->probeKicker,
                i == MISC_HW_KICKER_C);
        }

        // Kicker lists of the VAB codes 0x81 / 0x89
        for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
            BOOST_CHECK_EQUAL(manned(i)->kickersWithCapsule.size(),
                              (i == MANNED_HW_TWO_MAN_CAPSULE
                               || i == MANNED_HW_MINISHUTTLE) ? 2u : 0u);
            BOOST_CHECK_EQUAL(manned(i)->kickersSecondPad.size(),
                              i == MANNED_HW_THREE_MAN_CAPSULE ? 0u : 2u);
        }
    }
}

// Role helpers on real Equipment replace name/ID checks in mc2.cpp,
// mis_c.cpp and mis_m.cpp.
BOOST_AUTO_TEST_CASE(role_helpers_replace_name_checks)
{
    std::unique_ptr<Players> players(new Players());
    BuildStartState(*players, "CLASSIC", StartModel::Basic);

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        const BuzzData& p = players->P[plr];

        // strncmp(Name, "DO", 2) / "DOC"
        BOOST_CHECK(HasRole(p.Misc[MISC_HW_DOCKING_MODULE],
                            HardwareRole::DockingModule));
        BOOST_CHECK_EQUAL(strncmp(p.Misc[MISC_HW_DOCKING_MODULE].Name, "DO", 2), 0);
        // strncmp(Name, "PHO", 3)
        BOOST_CHECK(HasRole(p.Misc[MISC_HW_PHOTO_RECON],
                            HardwareRole::PhotoRecon));
        // ID "M2" / "M3"
        BOOST_CHECK(IsProbeKicker(p.Misc[MISC_HW_KICKER_C]));
        BOOST_CHECK(!IsProbeKicker(p.Misc[MISC_HW_KICKER_A]));
        BOOST_CHECK(HasRole(p.Misc[MISC_HW_EVA_SUITS], HardwareRole::Eva));
        // ID[0] == 'P'
        for (int i = 0; i < PROBE_HW_COUNT; i++) {
            BOOST_CHECK(IsProbe(p.Probe[i]));
        }

        BOOST_CHECK(!IsProbe(p.Misc[MISC_HW_DOCKING_MODULE]));
        // Minishuttle / Vostok-class / direct ascent by name or ID
        BOOST_CHECK(HasRole(p.Manned[MANNED_HW_MINISHUTTLE],
                            HardwareRole::Minishuttle));
        BOOST_CHECK(IsOneManCapsule(p.Manned[MANNED_HW_ONE_MAN_CAPSULE]));
        BOOST_CHECK(!IsOneManCapsule(p.Manned[MANNED_HW_ONE_MAN_MODULE]));
        BOOST_CHECK(IsDirectAscentCapsule(p.Manned[MANNED_HW_FOUR_MAN_CAPSULE]));
        BOOST_CHECK(!IsDirectAscentCapsule(p.Manned[MANNED_HW_THREE_MAN_CAPSULE]));

        for (int i = MANNED_HW_TWO_MAN_MODULE; i <= MANNED_HW_ONE_MAN_MODULE; i++) {
            BOOST_CHECK(HasRole(p.Manned[i], HardwareRole::Lander));
        }

        // Crew capacity per astronaut program (was {0,1,2,3,3,4})
        const int expected[6] = {0, 1, 2, 3, 3, 4};

        for (int prog = 0; prog < 6; prog++) {
            BOOST_CHECK_EQUAL(ProgramCrewCapacity(prog), expected[prog]);
        }
    }
}

// ---------------------------------------------------------------------
// (c) anim_id agrees with the keys used in seq.json / fseq.json
// ---------------------------------------------------------------------

// Mission step names are step letter + side letter (U/S) + ID, e.g.
// "ASR0" = step A, Soviet, R0.  The keys sit after the 3 character
// prefix of MissionIdSequence.
BOOST_AUTO_TEST_CASE(anim_ids_match_sequence_keys)
{
    std::set<std::pair<char, std::string>> used;
    const std::regex pattern("^.([US])([PRCM][0-9]).*");

    for (const char* file : {"seq.json", "fseq.json"}) {
        std::string path = locate_file(file, FT_DATA);
        BOOST_REQUIRE(!path.empty());
        Json::Value doc;
        Json::CharReaderBuilder rb;
        std::string errs;
        std::istringstream in(ReadFile(path));
        BOOST_REQUIRE_MESSAGE(Json::parseFromStream(rb, in, &doc, &errs),
                              file << ": " << errs);

        for (const Json::Value& item : doc["value0"]) {
            std::string id = item["MissionIdSequence"].asString();

            if (id.size() <= 3) {
                continue;
            }

            std::smatch m;
            std::string key = id.substr(3);

            if (std::regex_match(key, m, pattern)) {
                used.insert({m[1].str()[0], m[2].str()});
            }
        }
    }

    BOOST_CHECK(!used.empty());

    const HardwareCatalog& cat = GetHardwareCatalog();
    std::set<std::pair<char, std::string>> known;
    std::set<std::string> unused;

    for (const HardwareEntry& e : cat.entries()) {
        char sideLetter = (e.player == 0) ? 'U' : 'S';
        known.insert({sideLetter, e.animId});

        // anim_id equals the original ID characters (test-only entries
        // reuse the animations of other programs)
        if (e.legacyCat >= 0) {
            BOOST_CHECK_EQUAL(e.animId,
                              std::string(1, CATEGORY_ID[e.legacyCat])
                              + std::to_string(e.legacyIdx));
        }

        if (!used.count({sideLetter, e.animId})) {
            unused.insert(e.id);
        }
    }

    // Everything the sequence files reference exists in the catalog,
    // except R5/R6, which are rocket + booster combinations (the animation
    // id of the rocket with the booster offset 4 added, see mc2.cpp), not
    // programs.
    for (const auto& u : used) {
        if (u.second == "R5" || u.second == "R6") {
            continue;
        }

        BOOST_CHECK_MESSAGE(known.count(u),
                            "sequence key " << u.first << u.second
                            << " has no catalog entry");
    }

    // Programs without any clip: the US Kicker-C and the photo recon
    // satellites (no clips exist in the classic game).
    const std::set<std::string> expectedUnused = {
        "US.KICKER_C", "US.PHOTO_RECON", "SV.PHOTO_RECON"
    };
    BOOST_CHECK(unused == expectedUnused);
}


BOOST_AUTO_TEST_CASE(abort_system_assignment_and_validation)
{
    const HardwareCatalog& cat = GetHardwareCatalog();
    const std::map<std::string, std::string> expected = {
        {"US.MERCURY", "escape_tower"}, {"US.GEMINI", "ejection_seats"},
        {"US.APOLLO", "escape_tower"}, {"US.XMS2", "ejection_seats"},
        {"US.JUPITER", "escape_tower"}, {"US.EAGLE", "ascent_stage"},
        {"US.CRICKET", "ascent_stage"}, {"SV.VOSTOK", "ejection_seats"},
        {"SV.VOSKHOD", "none"}, {"SV.SOYUZ", "escape_tower"},
        {"SV.LAPOT", "ejection_seats"}, {"SV.LK700", "escape_tower"},
        {"SV.LKM", "ascent_stage"}, {"SV.LK", "ascent_stage"}
    };

    for (const auto& kv : expected) {
        const HardwareEntry* e = cat.FindById(kv.first);
        BOOST_REQUIRE_MESSAGE(e != nullptr, kv.first);
        BOOST_CHECK_MESSAGE(e->abortSystem == kv.second, kv.first);
    }

    for (const HardwareEntry& e : cat.entries()) {
        if (expected.count(e.id) == 0) {
            BOOST_CHECK_MESSAGE(e.abortSystem.empty(), e.id);
        }
    }

    auto setSystem = [](const char* id, const char* value) {
        return Mutate([=](Json::Value& d) {
            for (Json::Value& e : d["hardware"]) {
                if (e["id"] == id) {
                    e["abort_system"] = value;
                }
            }
        });
    };

    BOOST_CHECK_NO_THROW(HardwareCatalog::FromJsonText(setSystem("US.GEMINI", "none")));
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(setSystem("US.GEMINI", "parachute")),
                      std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(setSystem("US.GEMINI", "ascent_stage")),
                      std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(setSystem("US.EAGLE", "escape_tower")),
                      std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(setSystem("US.ATLAS", "escape_tower")),
                      std::runtime_error);
    BOOST_CHECK_THROW(HardwareCatalog::FromJsonText(Mutate([](Json::Value& d) {
        for (Json::Value& e : d["hardware"]) {
            if (e["id"] == "US.MERCURY") {
                e.removeMember("abort_system");
            }
        }
    })), std::runtime_error);
}

// 4c: schema extensions (validation only, no mechanics yet)
BOOST_AUTO_TEST_CASE(schema_extensions_are_validated)
{
    // Append a copy of an entry with changes.
    auto addCopy = [](Json::Value& d, const char* from, const char* id,
                      const std::function<void(Json::Value&)>& change) {
        Json::Value e;

        for (const Json::Value& v : d["hardware"]) {
            if (v["id"].asString() == from) {
                e = v;
            }
        }

        BOOST_REQUIRE(e.isObject());
        e["id"] = id;
        e.removeMember("legacy");
        e.removeMember("tech_transfer");
        e.removeMember("researchable");
        change(e);
        d["hardware"].append(e);
    };
    auto good = [&](const std::function<void(Json::Value&)>& change) {
        try {
            HardwareCatalog::FromJsonText(Mutate(change));
        } catch (const std::exception& ex) {
            BOOST_ERROR("rejected: " << ex.what());
        }
    };
    auto bad = [&](const char* what,
                   const std::function<void(Json::Value&)>& change) {
        BOOST_CHECK_MESSAGE(
            [&] {
                try {
                    HardwareCatalog::FromJsonText(Mutate(change));
                } catch (const std::runtime_error&) {
                    return true;
                }
                return false;
            }(), what);
    };

    // A spaceplane with a lab, a station and typed probes are accepted.
    good([&](Json::Value& d) {
        addCopy(d, "US.MERCURY", "US.X15", [](Json::Value& e) {
            e["role"] = "spaceplane";
            e["launch_mode"] = "air_launch";
            e["consumed_on_flight"] = false;
            e["compatible_labs"].append("US.LAB");
            e["compatible_landers"] = Json::Value(Json::arrayValue);
            e["lander_needs_docking_module"] = false;
        });
        addCopy(d, "US.EVA_SUITS", "US.LAB", [](Json::Value& e) {
            e["role"] = "lab_module";
            e["consumed_on_flight"] = false;
        });
        addCopy(d, "US.EVA_SUITS", "US.STATION", [](Json::Value& e) {
            e["role"] = "station";
            e["crew"] = 3;
            e["consumed_on_flight"] = false;
        });
        addCopy(d, "US.RANGER", "US.FLYBY", [](Json::Value& e) {
            e["probe_type"] = "flyby";
            e["targets"].append("venus");
            e["targets"].append("mars");
        });
        addCopy(d, "US.EXPLORER", "US.TIROS", [](Json::Value& e) {
            e["probe_type"] = "satellite";
            e["satellite_purpose"] = "weather";
            e["targets"].append("earth");
        });
    });

    bad("launch_mode unknown", [&](Json::Value& d) {
        d["hardware"][8]["launch_mode"] = "catapult";
    });
    bad("air_launch for a rocket", [&](Json::Value& d) {
        d["hardware"][3]["launch_mode"] = "air_launch";
    });
    bad("rocket not consumed", [&](Json::Value& d) {
        d["hardware"][3]["consumed_on_flight"] = false;
    });
    bad("probe not consumed", [&](Json::Value& d) {
        d["hardware"][0]["consumed_on_flight"] = false;
    });
    bad("probe_type unknown", [&](Json::Value& d) {
        d["hardware"][0]["probe_type"] = "teleporter";
    });
    bad("probe_type on a rocket", [&](Json::Value& d) {
        d["hardware"][3]["probe_type"] = "flyby";
    });
    bad("targets without probe_type", [&](Json::Value& d) {
        d["hardware"][0]["targets"].append("moon");
    });
    bad("unknown target", [&](Json::Value& d) {
        d["hardware"][0]["probe_type"] = "flyby";
        d["hardware"][0]["targets"].append("pluto");
    });
    bad("satellite_purpose for a flyby", [&](Json::Value& d) {
        d["hardware"][0]["probe_type"] = "flyby";
        d["hardware"][0]["satellite_purpose"] = "weather";
    });
    bad("unknown satellite_purpose", [&](Json::Value& d) {
        d["hardware"][0]["probe_type"] = "satellite";
        d["hardware"][0]["satellite_purpose"] = "spying";
    });
    bad("compatible_labs: unknown id", [&](Json::Value& d) {
        d["hardware"][8]["compatible_labs"].append("US.NO_LAB");
    });
    bad("compatible_labs: not a lab", [&](Json::Value& d) {
        d["hardware"][8]["compatible_labs"].append("US.GEMINI");
    });
    bad("compatible_labs on a probe", [&](Json::Value& d) {
        d["hardware"][0]["compatible_labs"].append("US.GEMINI");
    });
    bad("kicker_on_part unknown", [&](Json::Value& d) {
        d["hardware"][9]["kicker_on_part"] = "sometimes";
    });
    bad("kicker_on_part without landers", [&](Json::Value& d) {
        d["hardware"][8]["kicker_on_part"] = "all";
    });
    bad("role does not fit category", [&](Json::Value& d) {
        d["hardware"][0]["role"] = "station";
    });
    bad("station on the wrong role list", [&](Json::Value& d) {
        d["hardware"][3]["role"] = "spaceplane";
    });
}

// 4c: kicker_on_part replaces the rule derived from the docking module
BOOST_AUTO_TEST_CASE(kicker_on_part_matches_former_rule)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (const HardwareEntry& e : cat.entries()) {
        if (e.compatibleLanders.empty()) {
            BOOST_CHECK_EQUAL(e.kickerOnPart, "second_pad");
            continue;
        }

        BOOST_CHECK_MESSAGE(
            (e.kickerOnPart == "all") == e.landerNeedsDockingModule,
            e.id << " kicker_on_part");
    }
}

// 4c: the rd screens skip the photo recon and the Kicker C of the USA
BOOST_AUTO_TEST_CASE(researchable_flag)
{
    const HardwareCatalog& cat = GetHardwareCatalog();

    for (const HardwareEntry& e : cat.entries()) {
        const bool expected = !(e.role == HardwareRole::PhotoRecon
                                || e.id == "US.KICKER_C");
        BOOST_CHECK_MESSAGE(e.researchable == expected,
                            e.id << " researchable");
    }
}

BOOST_AUTO_TEST_SUITE_END()
