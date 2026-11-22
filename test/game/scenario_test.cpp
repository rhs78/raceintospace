/**
 * Tests of the scenarios and the dynamic game state (stage 4c):
 * scenario loading and validation, the start state built from a scenario,
 * save game and undo round trips with long hardware lists, the rejection
 * of old save games, and the research/purchase cycling over the lists.
 */
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include <json/json.h>

#include "game/data.h"
#include "game/fs.h"
#include "game/game_main.h"
#include "game/hardware.h"
#include "game/hardware_catalog.h"
#include "game/mission_util.h"
#include "game/options.h"
#include "game/proto.h"
#include "game/scenario.h"
#include "game/serialize.h"
#include "game/state_utils.h"
#include "game/vab.h"


namespace
{

std::unique_ptr<Players> NewState(const std::string& scenario,
                                  StartModel model = StartModel::Basic)
{
    std::unique_ptr<Players> p(new Players());
    BuildStartState(*p, scenario, model);
    return p;
}

std::string ReadFile(const std::string& path)
{
    std::ifstream in(path);
    BOOST_REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string ScenarioText(const char* name)
{
    const std::string path =
        locate_file((std::string("scenarios/") + name + ".json").c_str(),
                    FT_DATA);
    BOOST_REQUIRE(!path.empty());
    return ReadFile(path);
}

/** Parse the classic scenario, apply a change, write it again. */
std::string Mutate(const std::function<void(Json::Value&)>& change)
{
    Json::Value doc;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream in(ScenarioText("classic"));
    BOOST_REQUIRE(Json::parseFromStream(rb, in, &doc, &errs));
    change(doc);
    Json::StreamWriterBuilder wb;
    return Json::writeString(wb, doc);
}

bool Rejected(const std::function<void(Json::Value&)>& change)
{
    try {
        Scenario::FromJsonText(Mutate(change), GetHardwareCatalog());
    } catch (const std::runtime_error&) {
        return true;
    }

    return false;
}

/** Temporarily replaces the global Data. */
struct DataScope {
    Players* saved;
    std::unique_ptr<Players> players;

    explicit DataScope(std::unique_ptr<Players> p)
        : saved(Data), players(std::move(p))
    {
        Data = players.get();
    }

    ~DataScope() { Data = saved; }
};

std::string VasSnapshot()
{
    std::ostringstream o;
    o << "n=" << VASqty;

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 4; j++) {
            const VInfo& v = VAS[i][j];
            o << " [" << i << "," << j << ":" << v.name << " q" << int(v.qty)
              << " wt" << v.wt << " dex" << int(v.dex) << " img" << int(v.img)
              << "]";
        }
    }

    return o.str();
}

} // namespace


BOOST_AUTO_TEST_SUITE(scenario_suite)

BOOST_AUTO_TEST_CASE(classic_scenario_loads)
{
    const Scenario& s = GetScenario("CLASSIC");
    BOOST_CHECK_EQUAL(s.id(), "CLASSIC");
    BOOST_CHECK(!s.hidden());
    BOOST_CHECK_EQUAL(s.startYear(), 57);
    BOOST_CHECK_EQUAL(s.endYear(), 63);
    BOOST_CHECK_EQUAL(s.count(PROBE_HARDWARE), 3);
    BOOST_CHECK_EQUAL(s.count(ROCKET_HARDWARE), 5);
    BOOST_CHECK_EQUAL(s.count(MANNED_HARDWARE), 7);
    BOOST_CHECK_EQUAL(s.count(MISC_HARDWARE), 6);

    const HardwareCatalog& cat = GetHardwareCatalog();

    // The lists hold the classic programs in the classic order.
    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = 0; c < 4; c++) {
            const std::vector<int>& list = s.list(plr, c);

            for (size_t i = 0; i < list.size(); i++) {
                const HardwareEntry* e = cat.At(list[i]);
                BOOST_REQUIRE(e != nullptr);
                BOOST_CHECK_EQUAL(e->legacyCat, c);
                BOOST_CHECK_EQUAL(e->legacyIdx, int(i));
                BOOST_CHECK_EQUAL(e->player, plr);
            }
        }
    }

    // Lookups are case-insensitive and the scenario is cached.
    BOOST_CHECK(&GetScenario("classic") == &s);
    BOOST_CHECK_THROW(GetScenario("NO_SUCH_SCENARIO"), std::runtime_error);
    BOOST_CHECK_THROW(GetScenario("../hardware"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(scenario_listing_hides_test_scenario)
{
    const std::vector<std::string> shown = ListScenarios(false);
    BOOST_REQUIRE_EQUAL(shown.size(), 1u);
    BOOST_CHECK_EQUAL(shown[0], "CLASSIC");

    const std::vector<std::string> all = ListScenarios(true);
    BOOST_CHECK(std::find(all.begin(), all.end(), "TEST_EXTRA") != all.end());
    BOOST_CHECK_EQUAL(all.front(), "CLASSIC");
    BOOST_CHECK(GetScenario("TEST_EXTRA").hidden());
}

BOOST_AUTO_TEST_CASE(test_extra_scenario_adds_entries_at_list_end)
{
    const Scenario& s = GetScenario("TEST_EXTRA");
    const Scenario& c = GetScenario("CLASSIC");
    const HardwareCatalog& cat = GetHardwareCatalog();
    BOOST_CHECK_EQUAL(s.count(PROBE_HARDWARE), 4);
    BOOST_CHECK_EQUAL(s.count(ROCKET_HARDWARE), 6);
    BOOST_CHECK_EQUAL(s.count(MANNED_HARDWARE), 7);
    BOOST_CHECK_EQUAL(s.count(MISC_HARDWARE), 6);
    BOOST_CHECK_EQUAL(s.startYear(), c.startYear());

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int cc = 0; cc < 4; cc++) {
            const std::vector<int>& a = s.list(plr, cc);
            const std::vector<int>& b = c.list(plr, cc);
            BOOST_REQUIRE_GE(a.size(), b.size());
            BOOST_CHECK(std::equal(b.begin(), b.end(), a.begin()));
        }

        const HardwareEntry* rocket = cat.At(s.list(plr, ROCKET_HARDWARE)[5]);
        BOOST_CHECK(rocket->testOnly);
        BOOST_CHECK(rocket->role == HardwareRole::Launcher);
        const HardwareEntry* probe = cat.At(s.list(plr, PROBE_HARDWARE)[3]);
        BOOST_CHECK(probe->testOnly);
    }
}

BOOST_AUTO_TEST_CASE(scenario_validation_rejects_bad_input)
{
    // sanity: the unmodified text is accepted
    BOOST_CHECK(!Rejected([](Json::Value&) {}));

    // unknown program id
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"]["US"]["rocket"][0] = "US.NO_SUCH_ROCKET";
    }));
    // a program of the other side
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"]["US"]["rocket"][0] = "SV.R7";
    }));
    // a program in the wrong category list
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"]["US"]["probe"][0] = "US.ATLAS";
    }));
    // listed twice
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"]["US"]["rocket"].append("US.ATLAS");
    }));
    // missing side / category
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"].removeMember("SV");
    }));
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["hardware"]["US"].removeMember("misc");
    }));
    // no start values for a listed program
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["start_stats"]["basic"].removeMember("US.ATLAS");
    }));
    // start values missing a field / out of range
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["start_stats"]["basic"]["US.ATLAS"].removeMember("init_cost");
    }));
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["start_stats"]["basic"]["US.ATLAS"]["safety"] = 120;
    }));
    // years
    BOOST_CHECK(Rejected([](Json::Value& d) { d["start_year"] = 40; }));
    BOOST_CHECK(Rejected([](Json::Value& d) { d["end_year"] = 50; }));
    BOOST_CHECK(Rejected([](Json::Value& d) { d["end_year"] = 120; }));
    // missing id
    BOOST_CHECK(Rejected([](Json::Value& d) { d.removeMember("id"); }));
    // random model: unknown role, unknown field, missing program
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["random"][0]["role"] = "no_such_role";
    }));
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["random"][0]["ops"][0]["field"] = "no_such_field";
    }));
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["random"][0]["ordinal"] = 9;
    }));
    BOOST_CHECK(Rejected([](Json::Value& d) {
        d["random"][0]["ops"][0]["fn"] = "no_such_fn";
    }));

    // Not JSON at all
    BOOST_CHECK_THROW(Scenario::FromJsonText("{", GetHardwareCatalog()),
                      std::runtime_error);
    // "extends" needs the parent
    BOOST_CHECK_THROW(Scenario::FromJsonText(ScenarioText("test_extra"),
                                             GetHardwareCatalog(), nullptr),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(start_state_of_scenario_is_consistent)
{
    for (const char* name : {"CLASSIC", "TEST_EXTRA"}) {
        const Scenario& s = GetScenario(name);
        std::unique_ptr<Players> d = NewState(name);
        BOOST_CHECK_EQUAL(d->Scenario, name);
        BOOST_CHECK_EQUAL(d->StartYear, s.startYear());
        BOOST_CHECK_EQUAL(d->EndYear, s.endYear());
        BOOST_CHECK_EQUAL(d->Year, s.startYear());
        BOOST_CHECK_NO_THROW(ValidateHardwareState(*d));

        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            for (int c = 0; c < 4; c++) {
                const std::vector<Equipment>& list =
                    HardwareList(d->P[plr], c);
                BOOST_REQUIRE_EQUAL(list.size(), s.list(plr, c).size());
                BOOST_CHECK_EQUAL(d->P[plr].Buy[c].size(), list.size());
                BOOST_CHECK_GE(d->P[plr].IntelHardwareTable[c].size(),
                               list.size());

                for (size_t i = 0; i < list.size(); i++) {
                    BOOST_CHECK_EQUAL(list[i].CatalogIndex,
                                      s.list(plr, c)[i]);
                    const HardwareEntry* e = CatalogEntryForEquipment(list[i]);
                    BOOST_REQUIRE(e != nullptr);
                    BOOST_CHECK_EQUAL(std::string(list[i].Name), e->name);
                }
            }

            // one crew row per crewed program plus the unused row 0
            BOOST_CHECK_EQUAL(ProgramCount(d->P[plr]), 5);
            BOOST_CHECK_EQUAL(d->P[plr].Crew.size(), 6u);
            BOOST_CHECK_EQUAL(d->P[plr].CrewCount.size(), 6u);
            BOOST_CHECK(d->P[plr].ObjectsInOrbit.empty());
        }
    }
}

BOOST_AUTO_TEST_CASE(test_extra_state_keeps_classic_values)
{
    std::unique_ptr<Players> c = NewState("CLASSIC");
    std::unique_ptr<Players> t = NewState("TEST_EXTRA");

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int cat = 0; cat < 4; cat++) {
            const auto& a = HardwareList(c->P[plr], cat);
            const auto& b = HardwareList(t->P[plr], cat);

            for (size_t i = 0; i < a.size(); i++) {
                BOOST_CHECK_EQUAL(std::string(a[i].Name),
                                  std::string(b[i].Name));
                BOOST_CHECK_EQUAL(a[i].Safety, b[i].Safety);
                BOOST_CHECK_EQUAL(a[i].InitCost, b[i].InitCost);
                BOOST_CHECK_EQUAL(a[i].MaxPay, b[i].MaxPay);
            }
        }

        BOOST_CHECK_EQUAL(HardwareList(t->P[plr], ROCKET_HARDWARE).size(), 6u);
        const Equipment& extra = HardwareList(t->P[plr], ROCKET_HARDWARE)[5];
        BOOST_CHECK_EQUAL(std::string(extra.Name), "TEST ROCKET");
        BOOST_CHECK_EQUAL(extra.MaxPay, 2000);
        BOOST_CHECK_EQUAL(extra.Num, PROGRAM_NOT_STARTED);
    }
}

// The random model works via roles: the classic part of the test scenario
// is randomized exactly like the classic scenario, the extra programs keep
// their basic values.
BOOST_AUTO_TEST_CASE(random_model_works_via_roles)
{
    for (int seed = 1; seed <= 3; seed++) {
        std::srand(seed);
        std::unique_ptr<Players> c = NewState("CLASSIC", StartModel::Random);
        std::srand(seed);
        std::unique_ptr<Players> t = NewState("TEST_EXTRA", StartModel::Random);
        std::unique_ptr<Players> basic = NewState("TEST_EXTRA");

        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            for (int cat = 0; cat < 4; cat++) {
                const auto& a = HardwareList(c->P[plr], cat);
                const auto& b = HardwareList(t->P[plr], cat);
                const auto& base = HardwareList(basic->P[plr], cat);

                for (size_t i = 0; i < a.size(); i++) {
                    BOOST_CHECK_EQUAL(a[i].InitCost, b[i].InitCost);
                    BOOST_CHECK_EQUAL(a[i].MaxSafety, b[i].MaxSafety);
                    BOOST_CHECK_EQUAL(a[i].UnitWeight, b[i].UnitWeight);
                    BOOST_CHECK_EQUAL(a[i].MaxPay, b[i].MaxPay);
                }

                for (size_t i = a.size(); i < b.size(); i++) {
                    BOOST_CHECK_EQUAL(b[i].InitCost, base[i].InitCost);
                    BOOST_CHECK_EQUAL(b[i].MaxSafety, base[i].MaxSafety);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(historical_model_replaces_values_in_place)
{
    std::unique_ptr<Players> d = NewState("CLASSIC");
    BOOST_CHECK_EQUAL(d->P[0].Probe[0].UnitWeight, 300);
    d->P[0].Cash = 77;
    ApplyStartModel(*d, GetScenario("CLASSIC"), StartModel::Historical);
    BOOST_CHECK_EQUAL(d->P[0].Probe[0].UnitWeight, 200);
    BOOST_CHECK_EQUAL(d->P[0].Cash, 77);  // only the hardware changes

    std::unique_ptr<Players> h = NewState("CLASSIC", StartModel::Historical);
    BOOST_CHECK_EQUAL(h->P[0].Probe[0].UnitWeight, 200);
}

// Save game round trip with more than 7 programs in every category.
BOOST_AUTO_TEST_CASE(save_round_trip_with_long_lists)
{
    std::unique_ptr<Players> d = NewState("TEST_EXTRA");

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        BuzzData& p = d->P[plr];

        for (int c = 0; c < 4; c++) {
            std::vector<Equipment>& list = HardwareList(p, c);
            const Equipment last = list.back();

            // Grow every list to 12 programs (the programs may repeat).
            while (list.size() < 12) {
                Equipment e = last;
                e.Safety = int16_t(20 + list.size());
                e.Num = int8_t(list.size());
                list.push_back(e);
            }
        }

        SizeHardwareTables(p);
        p.Buy[ROCKET_HARDWARE][11] = 0x42;
        p.Buy[MISC_HARDWARE][9] = 0x21;
        p.IntelHardwareTable[PROBE_HARDWARE][10] = 85;
        p.Cash = 100 + plr;
        p.Crew[2][1][3] = 12;
        p.CrewCount[2][1] = 4;
        OrbitalObject station;
        station.CatalogIndex = 3;
        station.LaunchYear = 60;
        station.Lifetime = 8;
        station.Crew = 3;
        p.ObjectsInOrbit.push_back(station);
    }

    const std::string saved = SerializeGameState(*d);

    std::unique_ptr<Players> copy(new Players());
    DeserializeGameState(*copy, saved);
    BOOST_CHECK(SerializeGameState(*copy) == saved);

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = 0; c < 4; c++) {
            const auto& a = HardwareList(d->P[plr], c);
            const auto& b = HardwareList(copy->P[plr], c);
            BOOST_REQUIRE_EQUAL(a.size(), 12u);
            BOOST_REQUIRE_EQUAL(b.size(), 12u);

            for (size_t i = 0; i < a.size(); i++) {
                BOOST_CHECK_EQUAL(a[i].CatalogIndex, b[i].CatalogIndex);
                BOOST_CHECK_EQUAL(a[i].Safety, b[i].Safety);
                BOOST_CHECK_EQUAL(std::string(a[i].Name),
                                  std::string(b[i].Name));
            }
        }

        BOOST_CHECK_EQUAL(copy->P[plr].Buy[ROCKET_HARDWARE][11], 0x42);
        BOOST_CHECK_EQUAL(copy->P[plr].Buy[MISC_HARDWARE][9], 0x21);
        BOOST_CHECK_EQUAL(copy->P[plr].IntelHardwareTable[PROBE_HARDWARE][10],
                          85);
        BOOST_CHECK_EQUAL(copy->P[plr].Crew[2][1][3], 12);
        BOOST_CHECK_EQUAL(copy->P[plr].CrewCount[2][1], 4);
        BOOST_REQUIRE_EQUAL(copy->P[plr].ObjectsInOrbit.size(), 1u);
        BOOST_CHECK_EQUAL(copy->P[plr].ObjectsInOrbit[0].Lifetime, 8);
        BOOST_CHECK_EQUAL(copy->P[plr].ObjectsInOrbit[0].Crew, 3);
    }

    BOOST_CHECK_EQUAL(copy->Scenario, "TEST_EXTRA");
    BOOST_CHECK_NO_THROW(ValidateHardwareState(*copy));
}

BOOST_AUTO_TEST_CASE(loaded_state_must_fit_catalog_and_scenario)
{
    std::unique_ptr<Players> d = NewState("CLASSIC");
    BOOST_CHECK_NO_THROW(ValidateHardwareState(*d));

    // program of the wrong side
    d->P[0].Rocket[0].CatalogIndex =
        GetHardwareCatalog().IndexOf("SV.R7");
    BOOST_CHECK_THROW(ValidateHardwareState(*d), std::out_of_range);
    d = NewState("CLASSIC");

    // invalid catalog index
    d->P[1].Probe[1].CatalogIndex = -1;
    BOOST_CHECK_THROW(ValidateHardwareState(*d), std::out_of_range);
    d = NewState("CLASSIC");

    // table of the wrong size
    d->P[0].Buy[ROCKET_HARDWARE].pop_back();
    BOOST_CHECK_THROW(ValidateHardwareState(*d), std::out_of_range);
    d = NewState("CLASSIC");

    // unknown scenario
    d->Scenario = "NO_SUCH_SCENARIO";
    BOOST_CHECK_THROW(ValidateHardwareState(*d), std::runtime_error);
}

// Save games of an older format are rejected before the state is touched.
BOOST_AUTO_TEST_CASE(old_save_games_are_rejected)
{
    std::unique_ptr<Players> d = NewState("CLASSIC");
    d->P[0].Cash = 321;
    std::string text = SerializeGameState(*d);

    const std::regex version("\"cereal_class_version\": ?3");
    std::smatch match;
    BOOST_REQUIRE(std::regex_search(text, match, version));
    // the first one belongs to the Players object
    std::string old = text;
    old.replace(match.position(0), match.length(0),
                "\"cereal_class_version\":2");

    std::unique_ptr<Players> target = NewState("CLASSIC");
    target->P[0].Cash = 5;
    BOOST_CHECK_THROW(DeserializeGameState(*target, old),
                      SavegameVersionError);
    BOOST_CHECK_EQUAL(target->P[0].Cash, 5);  // untouched

    // the current version loads
    DeserializeGameState(*target, text);
    BOOST_CHECK_EQUAL(target->P[0].Cash, 321);

    // garbage is an error too, never a crash
    BOOST_CHECK_THROW(DeserializeGameState(*target, "not json"),
                      std::exception);
    BOOST_CHECK_THROW(DeserializeGameState(*target, ""), std::exception);
}

// The undo of the purchase screens: serialize, change, restore.
BOOST_AUTO_TEST_CASE(undo_round_trip)
{
    DataScope scope(NewState("TEST_EXTRA"));
    Players& d = *Data;
    d.P[0].Cash = 90;
    d.P[0].Rocket[5].Num = PROGRAM_NOT_STARTED;
    const std::string before = SerializeGameState(d);

    // purchase something
    d.P[0].Cash = 40;
    d.P[0].Rocket[5].Num = 2;
    d.P[0].Rocket[5].Safety = 55;
    d.P[0].Buy[ROCKET_HARDWARE][5] = 9;
    BOOST_CHECK(SerializeGameState(d) != before);

    DeserializeGameState(d, before);
    BOOST_CHECK(SerializeGameState(d) == before);
    BOOST_CHECK_EQUAL(d.P[0].Cash, 90);
    BOOST_CHECK_EQUAL(d.P[0].Rocket[5].Num, PROGRAM_NOT_STARTED);
    BOOST_CHECK_EQUAL(d.P[0].Buy[ROCKET_HARDWARE][5], 0);
}

BOOST_AUTO_TEST_CASE(undo_file_round_trip)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "ris_undo_test";
    fs::create_directories(dir);
    std::string dirString = dir.string();
    char* savedDir = options.dir_savegame;
    options.dir_savegame = const_cast<char*>(dirString.c_str());

    {
        DataScope scope(NewState("TEST_EXTRA"));
        Data->P[1].Cash = 55;
        Data->P[1].Rocket[5].Safety = 31;
        const std::string before = SerializeGameState(*Data);
        BOOST_REQUIRE(WriteUndoFile("UNDO.TMP"));

        Data->P[1].Cash = 1;
        Data->P[1].Rocket[5].Safety = 99;
        BOOST_REQUIRE(ReadUndoFile("UNDO.TMP"));
        BOOST_CHECK(SerializeGameState(*Data) == before);
        BOOST_CHECK_EQUAL(Data->P[1].Cash, 55);

        // a missing file is no error, just false
        remove_savedat("UNDO.TMP");
        BOOST_CHECK(!ReadUndoFile("UNDO.TMP"));
    }

    options.dir_savegame = savedDir;
    std::error_code ec;
    fs::remove_all(dir, ec);
}

// Research and purchase screens: the arrow buttons cycle over the real
// lists, so the additional programs of a scenario are reached.
BOOST_AUTO_TEST_CASE(research_cycling_uses_list_sizes)
{
    std::unique_ptr<Players> c = NewState("CLASSIC");
    std::unique_ptr<Players> t = NewState("TEST_EXTRA");

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        const BuzzData& cp = c->P[plr];
        const BuzzData& tp = t->P[plr];

        // classic: wraps around the 5 rockets, 3 probes, 7 manned
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, ROCKET_HARDWARE, 4, +1), 0);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, ROCKET_HARDWARE, 0, -1), 4);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, PROBE_HARDWARE, 2, +1), 0);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, PROBE_HARDWARE, 0, -1), 2);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, MANNED_HARDWARE, 6, +1), 0);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(cp, MANNED_HARDWARE, 0, -1), 6);

        // the extra rocket and probe are in the cycle
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, ROCKET_HARDWARE, 4, +1), 5);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, ROCKET_HARDWARE, 5, +1), 0);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, ROCKET_HARDWARE, 0, -1), 5);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, ROCKET_HARDWARE, 5, -1), 4);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, PROBE_HARDWARE, 2, +1), 3);
        BOOST_CHECK_EQUAL(CycleResearchableUnit(tp, PROBE_HARDWARE, 3, +1), 0);
        BOOST_CHECK(IsResearchable(HardwareList(tp, ROCKET_HARDWARE)[5]));

        // misc: no photo recon; the USA has no Kicker C
        const int next[2][5] = {{1, 3, 0, 4, 0}, {1, 2, 3, 4, 0}};
        const int prev[2][5] = {{4, 0, 1, 1, 3}, {4, 0, 1, 2, 3}};

        for (int i = 0; i < 5; i++) {
            if (plr == 0 && i == MISC_HW_KICKER_C) {
                continue;
            }

            BOOST_CHECK_MESSAGE(
                CycleResearchableUnit(cp, MISC_HARDWARE, i, +1)
                == next[plr][i], "plr " << plr << " misc " << i << " next");
            BOOST_CHECK_MESSAGE(
                CycleResearchableUnit(cp, MISC_HARDWARE, i, -1)
                == prev[plr][i], "plr " << plr << " misc " << i << " prev");
        }

        BOOST_CHECK(!IsResearchable(cp.Misc[MISC_HW_PHOTO_RECON]));
        BOOST_CHECK_EQUAL(IsResearchable(cp.Misc[MISC_HW_KICKER_C]), plr == 1);

        for (int cat = 0; cat < 4; cat++) {
            BOOST_CHECK_EQUAL(FirstResearchableUnit(cp, cat), 0);
        }
    }
}

// The payload list of the VAB does not change with the additional programs.
BOOST_AUTO_TEST_CASE(vab_payloads_unchanged_by_extra_programs)
{
    std::string classicSnapshots;
    std::string extraSnapshots;

    for (int scenario = 0; scenario < 2; scenario++) {
        DataScope scope(NewState(scenario == 0 ? "CLASSIC" : "TEST_EXTRA"));
        std::string& out = scenario == 0 ? classicSnapshots : extraSnapshots;

        for (const mStr& m : GetMissionData()) {
            for (int plr = 0; plr < NUM_PLAYERS; plr++) {
                for (int part = 0; part < 2; part++) {
                    for (int prog = 0; prog < MANNED_PROGRAM_COUNT; prog++) {
                        BuildVAB(plr, m.Index, 1, part, prog);
                        out += VasSnapshot() + "\n";
                    }
                }
            }
        }
    }

    BOOST_CHECK(classicSnapshots == extraSnapshots);
}

// Tech transfer and AI purchase work on the programs found by catalog
// index; a program the scenario lacks contributes nothing.
BOOST_AUTO_TEST_CASE(tech_transfer_resolves_via_catalog_index)
{
    DataScope scope(NewState("TEST_EXTRA"));
    Data->P[0].Rocket[1].Safety = 80;  // Titan
    Data->P[0].Rocket[0].Num = PROGRAM_NOT_STARTED;
    ApplyTechTransfer(0, ROCKET_HARDWARE, 0, TechTransferMode::Gradual);
    BOOST_CHECK_GT(Data->P[0].Rocket[0].Safety, 10);

    // the extra rocket has no tech transfer: keeps its value
    Data->P[0].Rocket[5].Safety = 17;
    ApplyTechTransfer(0, ROCKET_HARDWARE, 5, TechTransferMode::Gradual);
    BOOST_CHECK_EQUAL(Data->P[0].Rocket[5].Safety, 17);
    BOOST_CHECK_EQUAL(Data->P[0].Rocket[5].Base, 17);
}

BOOST_AUTO_TEST_SUITE_END()
