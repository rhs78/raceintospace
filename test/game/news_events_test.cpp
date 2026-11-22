/**
 * Regression test for the crash in Steal() (news event 5/47) after the
 * hardware lists became dynamic (stage 4c): the news event functions in
 * news_sup.cpp walked the four hardware lists as one block of 28 entries.
 * They must work for lists shorter or longer than 7 entries.
 */
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>

#include "game/data.h"
#include "game/game_main.h"
#include "game/news_sup.h"
#include "game/scenario.h"


namespace
{

struct DataScope {
    Players* saved;
    std::unique_ptr<Players> players;

    explicit DataScope(const std::string& scenario, StartModel model)
        : saved(Data), players(new Players())
    {
        BuildStartState(*players, scenario, model);
        Data = players.get();

        // Start all programs, so that every program takes part.
        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            for (auto* list : { &Data->P[plr].Probe, &Data->P[plr].Rocket,
                                &Data->P[plr].Manned, &Data->P[plr].Misc }) {
                for (Equipment& e : *list) {
                    e.Num = 1;
                    e.Safety = e.Base + 10;
                }
            }
        }

        // Make the rival stronger in some programs, weaker in others.
        for (Equipment& e : Data->P[1].Rocket) {
            e.Safety += 5;
        }
    }

    ~DataScope() { Data = saved; }
};

void RunAllEventFunctions()
{
    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int prog = 0; prog <= 4; prog++) {
            for (int type : { 1, -1 }) {
                Steal(plr, prog, type);
                NMod(plr, prog, type, 5);
                RDMods(plr, prog, type, 1);
            }

            DamMod(plr, prog, 5, 3);
            SaveMods(plr, prog);
        }
    }
}

} // namespace


BOOST_AUTO_TEST_SUITE(news_events_suite)

BOOST_AUTO_TEST_CASE(event_functions_work_with_classic_lists)
{
    for (StartModel model : { StartModel::Basic, StartModel::Historical }) {
        DataScope scope("classic", model);
        BOOST_CHECK_NO_THROW(RunAllEventFunctions());
    }
}

BOOST_AUTO_TEST_CASE(event_functions_work_with_extra_entries)
{
    DataScope scope("test_extra", StartModel::Basic);
    BOOST_CHECK_NO_THROW(RunAllEventFunctions());
}

BOOST_AUTO_TEST_SUITE_END()
