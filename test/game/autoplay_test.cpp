/**
 * Regression tests for the out-of-bounds accesses found by the autoplay
 * runs (tools/autoplay.sh, doc/fork/autoplay.md): crews that were broken up
 * while a mission still referred to them, and missions without a rocket.
 */
#include <boost/test/unit_test.hpp>

#include <memory>

#include "game/astros.h"
#include "game/autoplay.h"
#include "game/data.h"
#include "game/game_main.h"
#include "game/hardware.h"
#include "game/news_suq.h"
#include "game/scenario.h"

int CheckIfMissionGo(char plr, char launchIdx);  // game_main.cpp


namespace
{

struct DataScope {
    Players* saved;
    char savedAI[2];
    std::unique_ptr<Players> players;

    DataScope() : saved(Data), players(new Players())
    {
        savedAI[0] = AI[0];
        savedAI[1] = AI[1];
        BuildStartState(*players, "classic", StartModel::Basic);
        Data = players.get();
    }

    ~DataScope()
    {
        Data = saved;
        AI[0] = savedAI[0];
        AI[1] = savedAI[1];
    }
};

/** A manned one-man mission on pad 0; crew 1 of program 1 has two members. */
void SetUpCrew(int plr, int prog, int crew, int count)
{
    BuzzData& p = Data->P[plr];

    for (int i = 0; i < count; i++) {
        p.Crew[prog][crew][i] = i + 1;
        p.Pool[i].Status = AST_ST_ACTIVE;
        p.Pool[i].Assign = prog;
        p.Pool[i].Crew = crew + 1;
    }

    p.CrewCount[prog][crew] = count;
}

MissionType& ManMission(int plr, int prog, int pCrew, int bCrew)
{
    MissionType& m = Data->P[plr].Mission[0];
    m = MissionType{};
    ClearMissionHardware(m);
    m.MissionCode = Mission_Earth_Orbital;
    m.Men = 1;
    m.Prog = prog;
    m.PCrew = pCrew;
    m.BCrew = bCrew;
    return m;
}

} // namespace


BOOST_AUTO_TEST_SUITE(autoplay_fixes_suite)

BOOST_AUTO_TEST_CASE(model_names)
{
    BOOST_CHECK_EQUAL(std::string(AutoplayModelName(0)), "basic");
    BOOST_CHECK_EQUAL(std::string(AutoplayModelName(1)), "historical");
    BOOST_CHECK_EQUAL(std::string(AutoplayModelName(2)), "random");
    BOOST_CHECK(AutoplayModelName(3) == nullptr);
}

BOOST_AUTO_TEST_CASE(check_flight_crews_breaks_only_the_members)
{
    DataScope scope;
    SetUpCrew(0, 1, 0, 2);
    Data->P[0].Pool[1].Status = AST_ST_DEAD;

    CheckFlightCrews(0);

    BOOST_CHECK_EQUAL(int(Data->P[0].CrewCount[1][0]), 0);
    BOOST_CHECK_EQUAL(int(Data->P[0].Crew[1][0][0]), 0);
    BOOST_CHECK_EQUAL(int(Data->P[0].Crew[1][0][1]), 0);
    BOOST_CHECK_EQUAL(int(Data->P[0].Pool[0].Assign), 0);
    BOOST_CHECK_EQUAL(int(Data->P[0].Pool[1].Assign), 0);
    // The empty slot after the last member must not have been taken as
    // astronaut 0 (Pool[-1]) -- an intact crew stays intact.
    SetUpCrew(0, 1, 1, 2);
    CheckFlightCrews(0);
    BOOST_CHECK_EQUAL(int(Data->P[0].CrewCount[1][1]), 2);
}

BOOST_AUTO_TEST_CASE(crew_cant_fly_without_crews)
{
    DataScope scope;
    AI[0] = 0;

    // No primary crew (PCrew 0) used to index CrewCount[prog][-1].
    ManMission(0, 1, 0, 0);
    // AI: scrubbed quietly (a human would get a help dialog).
    AI[0] = 1;
    BOOST_CHECK(MissionCrewCantFly(0, 0));

    // Primary crew without members, but a backup crew that has some.
    SetUpCrew(0, 1, 1, 1);
    ManMission(0, 1, 1, 2);
    BOOST_CHECK(!MissionCrewCantFly(0, 0));

    // A primary crew with members is fine.
    SetUpCrew(0, 1, 0, 1);
    ManMission(0, 1, 1, 0);
    BOOST_CHECK(!MissionCrewCantFly(0, 0));

    // Unmanned: no crew needed.
    MissionType& m = ManMission(0, 1, 0, 0);
    m.Men = 0;
    BOOST_CHECK(!MissionCrewCantFly(0, 0));
}

BOOST_AUTO_TEST_CASE(mission_go_check_without_rocket)
{
    DataScope scope;
    AI[0] = 1;

    // Hard[Mission_Rocket] == -1 means "no rocket" (rocket 0 is a rocket);
    // CheckIfMissionGo() used to read Rocket[-1].
    MissionType& m = ManMission(0, 1, 1, 0);
    SetUpCrew(0, 1, 0, 1);

    ClearMissionHardware(m);

    m.Hard[Mission_Rocket] = -1;
    BOOST_CHECK_EQUAL(CheckIfMissionGo(0, 0), 1);
}

BOOST_AUTO_TEST_SUITE_END()
