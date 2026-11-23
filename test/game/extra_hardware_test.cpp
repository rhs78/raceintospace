/**
 * Tests of the mission hardware of stage 4d: the additional rocket and
 * probe of the scenario test_extra (list positions beyond the classic
 * programs) can be assembled in the VAB, are launched in a mission
 * (MissionSetup, step names from the animation id of the hardware), are
 * released again, and are used by the AI's vehicle and payload choice.
 */
#include <boost/test/unit_test.hpp>

#include <cstring>
#include <memory>
#include <string>

#include "game/aimis.h"
#include "game/data.h"
#include "game/game_main.h"
#include "game/hardware.h"
#include "game/hardware_catalog.h"
#include "game/mc.h"
#include "game/mc2.h"
#include "game/mission_util.h"
#include "game/scenario.h"
#include "game/state_utils.h"
#include "game/vab.h"
#include "game/vehicle.h"
#include "game/vehicle_selector.h"


namespace
{

/** Temporarily replaces the global Data (a TEST_EXTRA start state) and AI[]. */
struct ExtraScope {
    Players* saved;
    char savedAI[2];
    std::unique_ptr<Players> players;

    ExtraScope() : saved(Data), players(new Players())
    {
        savedAI[0] = AI[0];
        savedAI[1] = AI[1];
        BuildStartState(*players, "TEST_EXTRA", StartModel::Basic);
        Data = players.get();
    }

    ~ExtraScope()
    {
        Data = saved;
        AI[0] = savedAI[0];
        AI[1] = savedAI[1];
    }
};

/** List position of the test_only program of a category (-1 if none). */
int TestUnit(int plr, int category)
{
    const std::vector<Equipment>& list = HardwareList(Data->P[plr], category);

    for (size_t i = 0; i < list.size(); i++) {
        const HardwareEntry* e = HardwareInfo(list[i]);

        if (e != nullptr && e->testOnly) {
            return int(i);
        }
    }

    return -1;
}

/** An unmanned mission on pad 0 with the pad ready. */
MissionType& SetUpMission(int plr, int code)
{
    Data->P[plr].LaunchFacility[0] = LAUNCHPAD_OPERATIONAL;
    MissionType& m = Data->P[plr].Mission[0];
    m = MissionType{};
    ClearMissionHardware(m);
    m.MissionCode = code;
    m.Men = 0;
    m.Prog = 0;
    return m;
}

/** The row of VAS with a probe of the given list position (0 if none). */
int RowWithProbe(int unit)
{
    for (int row = 1; row <= VASqty; row++) {
        if (!VAS[row][Mission_Probe_DM].docking
            && VAS[row][Mission_Probe_DM].dex == unit) {
            return row;
        }
    }

    return 0;
}

} // namespace


BOOST_AUTO_TEST_SUITE(extra_hardware_suite)

// The extra programs are at the list ends, behind the classic positions.
BOOST_AUTO_TEST_CASE(extra_programs_are_appended)
{
    ExtraScope scope;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        BOOST_CHECK_EQUAL(TestUnit(plr, PROBE_HARDWARE), PROBE_HW_COUNT);
        BOOST_CHECK_EQUAL(TestUnit(plr, ROCKET_HARDWARE), ROCKET_HW_COUNT);
    }
}

// Every orbital probe the player has is a payload of the satellite
// mission; other probe missions keep their single payload.
BOOST_AUTO_TEST_CASE(vab_offers_every_probe_of_the_role)
{
    ExtraScope scope;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        const int extra = TestUnit(plr, PROBE_HARDWARE);

        BuildVAB(plr, Mission_Orbital_Satellite, 1, 0, 0);
        BOOST_CHECK_EQUAL(VASqty, 2);
        BOOST_CHECK_EQUAL(int(VAS[1][Mission_Probe_DM].dex), PROBE_HW_ORBITAL);
        BOOST_CHECK_EQUAL(int(VAS[2][Mission_Probe_DM].dex), extra);
        BOOST_CHECK_EQUAL(std::string(VAS[2][Mission_Probe_DM].name),
                          "TEST PROBE");
        BOOST_CHECK(!VAS[2][Mission_Probe_DM].docking);
        BOOST_CHECK_EQUAL(RowWithProbe(extra), 2);

        BuildVAB(plr, Mission_Lunar_Probe, 1, 0, 0);
        BOOST_CHECK_EQUAL(VASqty, 1);
        BOOST_CHECK_EQUAL(int(VAS[1][Mission_Probe_DM].dex), PROBE_HW_LUNAR);
    }
}

// There are more payload rows than the classic seven when many probes of a
// role exist (the VAS table is not limited).
BOOST_AUTO_TEST_CASE(vab_rows_are_not_limited)
{
    ExtraScope scope;
    BuzzData& p = Data->P[0];

    // Ten more orbital probes: copies of the first one.
    for (int i = 0; i < 10; i++) {
        p.Probe.push_back(p.Probe[PROBE_HW_ORBITAL]);
    }

    BuildVAB(0, Mission_Orbital_Satellite, 1, 0, 0);
    BOOST_CHECK_EQUAL(VASqty, 12);
    BOOST_CHECK_GE(VAS.size(), 13u);

    for (int row = 1; row <= VASqty; row++) {
        BOOST_CHECK(VAS[row][Mission_Probe_DM].dex >= 0);
    }
}

// The launch vehicles: every launcher of the rocket list, then those with
// boosters; the extra rocket takes its list position, accepts no boosters.
BOOST_AUTO_TEST_CASE(vehicle_options_follow_the_rocket_list)
{
    ExtraScope scope;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        const int extra = TestUnit(plr, ROCKET_HARDWARE);
        const std::vector<VehicleOption> options = LaunchVehicleOptions(plr);

        // Atlas, Titan, Saturn, Nova, extra, Atlas+B, Titan+B, Saturn+B
        BOOST_REQUIRE_EQUAL(options.size(), 8u);
        const int expected[8][2] = {
            {ROCKET_HW_ONE_STAGE, 0}, {ROCKET_HW_TWO_STAGE, 0},
            {ROCKET_HW_THREE_STAGE, 0}, {ROCKET_HW_MEGA_STAGE, 0},
            {extra, 0},
            {ROCKET_HW_ONE_STAGE, 1}, {ROCKET_HW_TWO_STAGE, 1},
            {ROCKET_HW_THREE_STAGE, 1}
        };

        for (int i = 0; i < 8; i++) {
            BOOST_CHECK_EQUAL(options[i].rocket, expected[i][0]);
            BOOST_CHECK_EQUAL(options[i].boosters, expected[i][1] != 0);
        }

        // The selector offers them in turn and wraps around.
        VehicleSelector selector(plr);
        BOOST_CHECK_EQUAL(selector.count(), 8);
        Vehicle v = selector.start();

        for (int i = 0; i < 8; i++) {
            BOOST_CHECK_EQUAL(v.rocketIndex(), expected[i][0]);
            BOOST_CHECK_EQUAL(v.hasBoosters(), expected[i][1] != 0);
            v = selector.next();
        }

        BOOST_CHECK_EQUAL(v.rocketIndex(), ROCKET_HW_ONE_STAGE);

        // Boosters only for rockets that accept them; the extra rocket's
        // reach comes from the catalog.
        BOOST_CHECK_THROW(Vehicle(plr, extra, true), std::logic_error);
        BOOST_CHECK_THROW(Vehicle(plr, ROCKET_HW_BOOSTERS), std::logic_error);
        BOOST_CHECK(!Vehicle(plr, ROCKET_HW_ONE_STAGE).translunar());
        BOOST_CHECK(Vehicle(plr, ROCKET_HW_TWO_STAGE).translunar());
        BOOST_CHECK(Vehicle(plr, extra).translunar());
    }
}

// Assemble the extra rocket and probe, launch them, release them.
BOOST_AUTO_TEST_CASE(extra_hardware_is_assembled_and_launched)
{
    ExtraScope scope;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        BuzzData& p = Data->P[plr];
        const int probe = TestUnit(plr, PROBE_HARDWARE);
        const int rocket = TestUnit(plr, ROCKET_HARDWARE);

        // The extra hardware has been bought; its animation ids are
        // distinctive, to show that the step names come from them and not
        // from the list position.
        p.Probe[probe].Num = 2;
        p.Rocket[rocket].Num = 2;
        p.Probe[probe].ID[0] = 'P';
        p.Probe[probe].ID[1] = '2';
        p.Rocket[rocket].ID[0] = 'R';
        p.Rocket[rocket].ID[1] = '3';

        MissionType& m = SetUpMission(plr, Mission_Orbital_Satellite);
        BuildVAB(plr, 0, 0, 0, 0);
        const int row = RowWithProbe(probe);
        BOOST_REQUIRE_EQUAL(row, 2);

        Vehicle vehicle(plr, rocket);
        BOOST_CHECK(vehicle.started());
        BOOST_CHECK_EQUAL(vehicle.available(), 2);
        BOOST_CHECK_EQUAL(vehicle.thrust(), 2000);
        BOOST_CHECK(PayloadReady(plr, row));

        ReserveHardware(plr, 0, row, vehicle);
        BOOST_CHECK_EQUAL(int(m.Hard[Mission_Probe]), probe);
        BOOST_CHECK_EQUAL(int(m.Hard[Mission_Rocket]), rocket);
        BOOST_CHECK(!m.Booster);
        BOOST_CHECK_EQUAL(int(m.DockingModule), -1);
        BOOST_CHECK_EQUAL(int(m.Hard[Mission_Capsule]), -1);
        BOOST_CHECK(HasLaunchVehicle(m));
        BOOST_CHECK_EQUAL(int(p.Probe[probe].Spok), 1);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].Spok), 1);
        BOOST_CHECK_EQUAL(int(p.Probe[PROBE_HW_ORBITAL].Spok), 0);

        // Released again: nothing stays reserved.
        FreeLaunchHardware(plr, 0);
        BOOST_CHECK_EQUAL(int(p.Probe[probe].Spok), 0);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].Spok), 0);

        ClearMissionHardware(m);  // (the VAB does this before an assembly)
        ReserveHardware(plr, 0, row, vehicle);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].Spok), 1);

        // Launch: the hardware is set up for the mission steps.
        memset(MH, 0x00, sizeof MH);
        MissionSetup(plr, 0);
        BOOST_CHECK(MH[0][Mission_Rocket] == &p.Rocket[rocket]);
        BOOST_CHECK(MH[0][Mission_Probe_DM] == &p.Probe[probe]);
        BOOST_CHECK(MH[0][Mission_SecondaryBooster] == nullptr);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].Num), 1);
        BOOST_CHECK_EQUAL(int(p.Probe[probe].Num), 1);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].Used), 1);
        BOOST_CHECK_EQUAL(int(p.Probe[probe].Used), 1);
        BOOST_CHECK_EQUAL(int(p.Rocket[rocket].MisSaf),
                          int(p.Rocket[rocket].Safety));

        // The mission steps are named from the animation ids of the
        // hardware: A<side><rocket id> for the launch, B/C<side><probe id>
        // for the satellite steps.
        memset(Mev, 0x00, sizeof Mev);
        MANNED[0] = MANNED[1] = 0;
        MissionCodes(plr, Mission_Orbital_Satellite, 0);

        const char side = plr == 0 ? 'U' : 'S';
        bool launch = false;
        bool satellite = false;

        for (int i = 0; i < 20 && Mev[i].Name[0] != 0; i++) {
            const std::string name(Mev[i].Name);

            if (name[0] == 'A' && name.size() >= 4) {
                BOOST_CHECK_EQUAL(name.substr(0, 4),
                                  std::string("A") + side + "R3");
                BOOST_CHECK(Mev[i].Ep == &p.Rocket[rocket]);
                launch = true;
            }

            if ((name[0] == 'B' || name[0] == 'C') && name.size() >= 4) {
                BOOST_CHECK_EQUAL(name.substr(1, 3),
                                  std::string(1, side) + "P2");
                BOOST_CHECK(Mev[i].Ep == &p.Probe[probe]);
                satellite = true;
            }
        }

        BOOST_CHECK(launch);
        BOOST_CHECK(satellite);
    }
}

// A rocket with boosters: the flag and the booster program, not an encoded
// value, make the vehicle.
BOOST_AUTO_TEST_CASE(boosters_are_a_flag)
{
    ExtraScope scope;
    const int plr = 0;
    BuzzData& p = Data->P[plr];

    p.Rocket[ROCKET_HW_TWO_STAGE].Num = 1;
    p.Rocket[ROCKET_HW_BOOSTERS].Num = 1;

    MissionType& m = SetUpMission(plr, Mission_Orbital_Satellite);
    BuildVAB(plr, 0, 0, 0, 0);

    Vehicle vehicle(plr, ROCKET_HW_TWO_STAGE, true);
    BOOST_CHECK_EQUAL(vehicle.name().substr(0, 2), "B/");
    BOOST_CHECK_EQUAL(vehicle.sprite(), 5);  // Titan with boosters
    ReserveHardware(plr, 0, 1, vehicle);

    BOOST_CHECK_EQUAL(int(m.Hard[Mission_Rocket]), ROCKET_HW_TWO_STAGE);
    BOOST_CHECK(m.Booster);
    BOOST_CHECK_EQUAL(int(p.Rocket[ROCKET_HW_TWO_STAGE].Spok), 1);
    BOOST_CHECK_EQUAL(int(p.Rocket[ROCKET_HW_BOOSTERS].Spok), 1);

    FreeLaunchHardware(plr, 0);
    BOOST_CHECK_EQUAL(int(p.Rocket[ROCKET_HW_TWO_STAGE].Spok), 0);
    BOOST_CHECK_EQUAL(int(p.Rocket[ROCKET_HW_BOOSTERS].Spok), 0);

    // A mission without hardware has no launch vehicle and no booster.
    ClearMissionHardware(m);
    BOOST_CHECK(!HasLaunchVehicle(m));
    BOOST_CHECK(!m.Booster);
}

// The AI assembles with the extra hardware where it is the best choice:
// the safest payload probe, the safest rocket that lifts the payload.
BOOST_AUTO_TEST_CASE(ai_chooses_extra_hardware)
{
    ExtraScope scope;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        BuzzData& p = Data->P[plr];
        AI[plr] = 1;
        const int probe = TestUnit(plr, PROBE_HARDWARE);
        const int rocket = TestUnit(plr, ROCKET_HARDWARE);

        for (Equipment& e : p.Probe) {
            e.Num = 2;
            e.Safety = 60;
            e.MSF = 60;
        }

        for (Equipment& e : p.Rocket) {
            e.Num = 2;
            e.Safety = 70;
        }

        p.Probe[probe].Safety = 90;
        p.Rocket[rocket].Safety = 95;

        // A satellite mission: the extra probe is the best of the payloads
        // (the AI flies the Atlas class for satellites).
        MissionType& sat = SetUpMission(plr, Mission_Orbital_Satellite);
        AILaunch(plr);
        BOOST_CHECK_EQUAL(int(sat.Hard[Mission_Probe]), probe);
        BOOST_CHECK(HasLaunchVehicle(sat));

        // A manned orbital flight: no preferred rocket, the safest one that
        // lifts the capsule is the extra rocket.
        for (Equipment& e : p.Manned) {
            e.Num = 2;
            e.Safety = 80;
        }

        MissionType& orbital = SetUpMission(plr, Mission_Earth_Orbital);
        orbital.Men = 1;
        orbital.Prog = 1;
        AILaunch(plr);
        BOOST_CHECK_EQUAL(int(orbital.Hard[Mission_Capsule]), MANNED_HW_ONE_MAN_CAPSULE);
        BOOST_CHECK_EQUAL(int(orbital.Hard[Mission_Rocket]), rocket);
        BOOST_CHECK(!orbital.Booster);
    }
}

BOOST_AUTO_TEST_SUITE_END()
