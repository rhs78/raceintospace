/**
 * Characterization tests for the hardware model: the ID-based
 * classification of Equipment, the integer encodings used by
 * MissionType.Hard[], and loading/saving of the Players data.
 * These document current behaviour ahead of a refactoring.
 */
#include <boost/test/unit_test.hpp>

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "game/data.h"
#include "game/fs.h"
#include "game/game_main.h"
#include "game/hardware.h"
#include "game/scenario.h"
#include "game/serialize.h"


namespace
{

/** The classic program of the category letter and index (as the former ID
 *  characters), identified by its catalog index; none if not valid. */
Equipment MakeEquipment(char category, int index)
{
    Equipment eq{};
    eq.ID[0] = category;
    eq.ID[1] = static_cast<char>('0' + index);

    for (int c = 0; c < 4; c++) {
        if (category == "PRCM"[c]) {
            const HardwareEntry* e =
                GetHardwareCatalog().FindByLegacy(0, c, index);

            if (e != nullptr) {
                eq.CatalogIndex = GetHardwareCatalog().IndexOf(e->id);
            }
        }
    }

    return eq;
}

std::unique_ptr<Players> NewPlayers()
{
    std::unique_ptr<Players> players(new Players());
    return players;
}

/** The start state of the classic scenario (basic model). */
std::unique_ptr<Players> LoadUrast()
{
    std::unique_ptr<Players> players = NewPlayers();
    BuildStartState(*players, "CLASSIC", StartModel::Basic);
    return players;
}

std::string ToJson(const Players& players)
{
    std::stringstream stream;
    {
        cereal::JSONOutputArchive archive(stream);
        archive(cereal::make_nvp("Data", players));
    }
    return stream.str();
}

const char CATEGORY_ID[4] = {'P', 'R', 'C', 'M'};

} // namespace


BOOST_AUTO_TEST_SUITE(hardware_suite)

BOOST_AUTO_TEST_CASE(HardwareType_probes)
{
    for (int i = 0; i < PROBE_HW_COUNT; i++) {
        BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('P', i)),
                          Mission_Probe_DM);
    }
}

BOOST_AUTO_TEST_CASE(HardwareType_rockets)
{
    for (int i = 0; i < ROCKET_HW_BOOSTERS; i++) {
        BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('R', i)),
                          Mission_PrimaryBooster);
    }

    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('R', ROCKET_HW_BOOSTERS)),
                      Mission_SecondaryBooster);
}

BOOST_AUTO_TEST_CASE(HardwareType_manned)
{
    for (int i = MANNED_HW_ONE_MAN_CAPSULE;
         i <= MANNED_HW_FOUR_MAN_CAPSULE; i++) {
        BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('C', i)),
                          Mission_Capsule);
    }

    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('C', MANNED_HW_TWO_MAN_MODULE)),
                      Mission_LM);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('C', MANNED_HW_ONE_MAN_MODULE)),
                      Mission_LM);
}

BOOST_AUTO_TEST_CASE(HardwareType_misc)
{
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_KICKER_A)),
                      Mission_Kicker);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_KICKER_B)),
                      Mission_Kicker);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_KICKER_C)),
                      Mission_Kicker);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_EVA_SUITS)),
                      Mission_EVA);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_DOCKING_MODULE)),
                      Mission_Probe_DM);
    BOOST_CHECK_EQUAL(HardwareType(MakeEquipment('M', MISC_HW_PHOTO_RECON)),
                      Mission_PhotoRecon);
}

BOOST_AUTO_TEST_CASE(HardwareType_invalid_throws)
{
    BOOST_CHECK_THROW(HardwareType(MakeEquipment('M', 6)),
                      std::invalid_argument);
    BOOST_CHECK_THROW(HardwareType(MakeEquipment('X', 0)),
                      std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(CrewSize_manned)
{
    const int expected[MANNED_HW_COUNT] = {1, 2, 3, 3, 4, 2, 1};

    for (int i = 0; i < MANNED_HW_COUNT; i++) {
        BOOST_CHECK_MESSAGE(CrewSize(MakeEquipment('C', i)) == expected[i],
                            "CrewSize(C" << i << ")");
    }
}

BOOST_AUTO_TEST_CASE(CrewSize_unmanned_is_zero)
{
    for (int i = 0; i < PROBE_HW_COUNT; i++) {
        BOOST_CHECK_EQUAL(CrewSize(MakeEquipment('P', i)), 0);
    }

    for (int i = 0; i < ROCKET_HW_COUNT; i++) {
        BOOST_CHECK_EQUAL(CrewSize(MakeEquipment('R', i)), 0);
    }

    for (int i = 0; i < MISC_HW_COUNT; i++) {
        BOOST_CHECK_EQUAL(CrewSize(MakeEquipment('M', i)), 0);
    }
}

BOOST_AUTO_TEST_CASE(HardwareCount_per_category)
{
    BOOST_CHECK_EQUAL(HardwareCount(PROBE_HARDWARE), 3);
    BOOST_CHECK_EQUAL(HardwareCount(ROCKET_HARDWARE), 5);
    BOOST_CHECK_EQUAL(HardwareCount(MANNED_HARDWARE), 7);
    BOOST_CHECK_EQUAL(HardwareCount(MISC_HARDWARE), 6);
    BOOST_CHECK_THROW(HardwareCount(4), std::invalid_argument);
    BOOST_CHECK_THROW(HardwareCount(-1), std::invalid_argument);
}

// Mission hardware (stage 4d): list positions, the boosters are a flag and
// the docking module has its own field.
BOOST_AUTO_TEST_CASE(mission_hardware_is_cleared_to_minus_one)
{
    MissionType mission;
    memset(&mission, 0x00, sizeof mission);
    ClearMissionHardware(mission);

    for (int i = 0; i <= Mission_EVA; i++) {
        BOOST_CHECK_EQUAL(int(mission.Hard[i]), -1);
    }

    BOOST_CHECK_EQUAL(int(mission.DockingModule), -1);
    BOOST_CHECK(!mission.Booster);
    BOOST_CHECK(!HasLaunchVehicle(mission));

    // rocket 0 is a rocket, not "no rocket"
    mission.Hard[Mission_Rocket] = 0;
    BOOST_CHECK(HasLaunchVehicle(mission));
}

BOOST_AUTO_TEST_CASE(role_lookups)
{
    Players* saved = Data;
    std::unique_ptr<Players> players = LoadUrast();
    Data = players.get();

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        BOOST_CHECK_EQUAL(DockingModuleIndex(plr), MISC_HW_DOCKING_MODULE);
        BOOST_CHECK_EQUAL(BoosterIndex(plr), ROCKET_HW_BOOSTERS);
        BOOST_CHECK_EQUAL(EvaSuitsIndex(plr), MISC_HW_EVA_SUITS);
        BOOST_CHECK_EQUAL(HardwareIndicesByRole(plr, HardwareRole::OrbitalProbe).size(), 1u);
        BOOST_CHECK_EQUAL(HardwareIndexByRole(plr, HardwareRole::LunarProbe),
                          PROBE_HW_LUNAR);
        BOOST_CHECK_EQUAL(ProbeImageSlot(plr, PROBE_HW_INTERPLANETARY), 1);
        BOOST_CHECK_EQUAL(ProbeImageSlot(plr, -1), -1);
    }

    Data = saved;
}

BOOST_AUTO_TEST_CASE(mission_equipment_resolves_the_slots)
{
    Players* saved = Data;
    std::unique_ptr<Players> players = LoadUrast();
    Data = players.get();
    BuzzData& p = Data->P[0];

    MissionType mission;
    memset(&mission, 0x00, sizeof mission);
    ClearMissionHardware(mission);
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Rocket) == nullptr);

    mission.Hard[Mission_Capsule] = MANNED_HW_TWO_MAN_CAPSULE;
    mission.Hard[Mission_Rocket] = ROCKET_HW_TWO_STAGE;
    mission.Booster = true;
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Capsule)
                == &p.Manned[MANNED_HW_TWO_MAN_CAPSULE]);
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Rocket)
                == &p.Rocket[ROCKET_HW_TWO_STAGE]);
    BOOST_CHECK(MissionEquipment(0, mission, Mission_SecondaryBooster)
                == &p.Rocket[ROCKET_HW_BOOSTERS]);

    // payload slot: probe, else docking module
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Probe_DM) == nullptr);
    mission.DockingModule = MISC_HW_DOCKING_MODULE;
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Probe_DM)
                == &p.Misc[MISC_HW_DOCKING_MODULE]);
    mission.Hard[Mission_Probe] = PROBE_HW_LUNAR;
    BOOST_CHECK(MissionEquipment(0, mission, Mission_Probe_DM)
                == &p.Probe[PROBE_HW_LUNAR]);

    Data = saved;
}

BOOST_AUTO_TEST_CASE(program_manned_index)
{
    for (int prog = 1; prog <= 5; prog++) {
        BOOST_CHECK_EQUAL(ProgramToMannedIndex(prog), prog - 1);
        BOOST_CHECK_EQUAL(MannedIndexToProgram(ProgramToMannedIndex(prog)),
                          prog);
    }
}

// Loading the initial game state, as done in game_main.cpp.
BOOST_AUTO_TEST_CASE(urast_loads)
{
    std::unique_ptr<Players> players = LoadUrast();

    BOOST_CHECK_EQUAL(players->Scenario, "CLASSIC");
    BOOST_CHECK_EQUAL(std::string(players->P[0].Name), "WEBB");
    BOOST_CHECK_EQUAL(std::string(players->P[1].Name), "KOROLYOV");

    const char* const names[2][4][7] = {
        {
            {"EXPLORER", "RANGER", "SURVEYOR"},
            {"ATLAS", "TITAN", "SATURN", "NOVA", "BOOSTERS"},
            {"MERCURY", "GEMINI", "APOLLO", "XMS-2", "JUPITER", "EAGLE",
             "CRICKET"},
            {"KICKER-A", "KICKER-B", "KICKER-C", "EVA SUITS", "DOCKING",
             "PHOTO RECON"}
        },
        {
            {"SPUTNIK", "COSMOS", "LUNA"},
            {"R-7", "PROTON", "N1", "UR-700", "BOOSTERS"},
            {"VOSTOK", "VOSKHOD", "SOYUZ", "LAPOT", "LK-700", "LKM", "LK"},
            {"KICKER-A", "KICKER-B", "KICKER-C", "EVA SUITS", "DOCKING",
             "PHOTO RECON"}
        }
    };

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int cat = 0; cat < 4; cat++) {
            const std::vector<Equipment>& arr =
                HardwareList(players->P[plr], cat);
            BOOST_CHECK_EQUAL(int(arr.size()), HardwareCount(cat));

            for (int i = 0; i < HardwareCount(cat); i++) {
                const Equipment& eq = arr[i];
                BOOST_CHECK_MESSAGE(
                    std::string(eq.Name) == names[plr][cat][i],
                    "player " << plr << " category " << cat << " index "
                    << i << " is \"" << eq.Name << "\"");
                BOOST_CHECK_EQUAL(eq.ID[0], CATEGORY_ID[cat]);
                BOOST_CHECK_EQUAL(eq.ID[1], '0' + i);
            }
        }
    }
}

// The historical model (the former hist.json) is part of the scenario.
BOOST_AUTO_TEST_CASE(hist_loads)
{
    std::unique_ptr<Players> players = NewPlayers();
    BuildStartState(*players, "CLASSIC", StartModel::Historical);
    Players* Data = players.get();

    BOOST_CHECK_EQUAL(std::string(Data->P[0].Probe[0].Name), "EXPLORER");
    BOOST_CHECK_EQUAL(std::string(Data->P[0].Rocket[4].Name), "BOOSTERS");
    BOOST_CHECK_EQUAL(std::string(Data->P[0].Manned[6].Name), "CRICKET");
    BOOST_CHECK_EQUAL(std::string(Data->P[0].Misc[5].Name), "PHOTO RECON");
    BOOST_CHECK_EQUAL(std::string(Data->P[1].Probe[0].Name), "SPUTNIK");
    BOOST_CHECK_EQUAL(std::string(Data->P[1].Rocket[3].Name), "UR-700");
    BOOST_CHECK_EQUAL(std::string(Data->P[1].Manned[6].Name), "LK");
    BOOST_CHECK_EQUAL(std::string(Data->P[1].Misc[4].Name), "DOCKING");
    // historical values differ from the basic ones
    BOOST_CHECK_EQUAL(Data->P[0].Probe[0].UnitWeight, 200);

    // Every used slot has an ID matching its position.
    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int cat = 0; cat < 4; cat++) {
            const std::vector<Equipment>& arr =
                HardwareList(Data->P[plr], cat);

            for (int i = 0; i < HardwareCount(cat); i++) {
                BOOST_CHECK_EQUAL(arr[i].ID[0], CATEGORY_ID[cat]);
                BOOST_CHECK_EQUAL(arr[i].ID[1], '0' + i);
            }
        }
    }
}

// Same format as write_save_file(): a JSON round trip of Players
// must be lossless.
BOOST_AUTO_TEST_CASE(players_json_round_trip)
{
    std::unique_ptr<Players> original = LoadUrast();

    // Make the equipment distinguishable.
    original->P[0].Cash = 123;
    original->P[1].Manned[MANNED_HW_ONE_MAN_MODULE].Safety = 77;
    original->P[0].Rocket[ROCKET_HW_BOOSTERS].Num = 3;
    original->P[0].Mission[1].Hard[Mission_Rocket] = ROCKET_HW_TWO_STAGE;
    original->P[0].Mission[1].Booster = true;
    original->P[0].Mission[1].DockingModule = MISC_HW_DOCKING_MODULE;

    const std::string json = ToJson(*original);

    std::unique_ptr<Players> copy = NewPlayers();
    {
        std::stringstream stream(json);
        cereal::JSONInputArchive archive(stream);
        archive(cereal::make_nvp("Data", *copy));
    }

    BOOST_CHECK_EQUAL(copy->P[0].Cash, 123);
    BOOST_CHECK_EQUAL(copy->P[1].Manned[MANNED_HW_ONE_MAN_MODULE].Safety, 77);
    BOOST_CHECK_EQUAL(copy->P[0].Rocket[ROCKET_HW_BOOSTERS].Num, 3);
    BOOST_CHECK_EQUAL(copy->P[0].Mission[1].Hard[Mission_Rocket],
                      ROCKET_HW_TWO_STAGE);
    BOOST_CHECK(copy->P[0].Mission[1].Booster);
    BOOST_CHECK_EQUAL(int(copy->P[0].Mission[1].DockingModule),
                      MISC_HW_DOCKING_MODULE);
    BOOST_CHECK_EQUAL(int(copy->P[0].Mission[0].Hard[Mission_Rocket]), -1);
    BOOST_CHECK_EQUAL(std::string(copy->P[1].Misc[MISC_HW_DOCKING_MODULE].Name),
                      "DOCKING");
    BOOST_CHECK(ToJson(*copy) == json);
}

BOOST_AUTO_TEST_SUITE_END()
