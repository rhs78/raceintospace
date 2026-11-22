/**
 * Tests for the parameterized timeline: turn index helpers, launch
 * windows, event card initialization, year formatting, news indexing
 * and the save game round trip of the new fields.
 */
#include <boost/test/unit_test.hpp>

#include <cstring>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "game/data.h"
#include "game/fs.h"
#include "game/mission_util.h"
#include "game/serialize.h"
#include "game/timeline.h"


namespace
{

std::unique_ptr<Players> NewPlayers(int start, int end)
{
    std::unique_ptr<Players> p(new Players());
    std::memset(p.get(), 0, sizeof(Players));
    p->StartYear = start;
    p->EndYear = end;
    p->Year = start;
    p->Season = 0;
    return p;
}

// The hand-made table of the original game (1957-1977).
bool OldMars(int y, int s)
{
    return (y == 60 && s == 0) || (y == 62 && s == 0) ||
           (y == 64 && s == 0) || (y == 66 && s == 0) ||
           (y == 69 && s == 1) || (y == 71 && s == 1) ||
           (y == 73 && s == 1);
}

bool OldJupiter(int y, int)
{
    return y == 60 || y == 64 || y == 68 || y == 72 || y == 73 || y == 77;
}

bool OldSaturn(int y, int)
{
    return y == 61 || y == 66 || y == 72;
}

// Turns (as year * 2 + season) in 1957-1977 where the model and the
// old table disagree.
std::vector<int> Deviations(const LaunchWindowModel& m, bool (*old)(int, int))
{
    std::vector<int> d;

    for (int y = 57; y <= 77; y++) {
        for (int s = 0; s < 2; s++) {
            if (InLaunchWindow(m, y, s) != old(y, s)) {
                d.push_back(2 * y + s);
            }
        }
    }

    return d;
}

// The event card algorithm of the original game (start.cpp before the
// timeline change), for comparison.
std::vector<int> OriginalEventCards(const std::function<int(int)>& rnd, int cards)
{
    const int POOLS = 5, PERIODS = 6;
    std::vector<std::vector<int>> pool(POOLS);
    pool[0].resize(3);  std::iota(pool[0].begin(), pool[0].end(), 1);
    pool[1].resize(40); std::iota(pool[1].begin(), pool[1].end(), 4);
    pool[2].resize(19); std::iota(pool[2].begin(), pool[2].end(), 44);
    pool[3].resize(17); std::iota(pool[3].begin(), pool[3].end(), 63);
    pool[4].resize(20); std::iota(pool[4].begin(), pool[4].end(), 80);
    char quantity[PERIODS] = {2, 8, 8, 12, 16, 52};
    char pct[PERIODS][POOLS] = {
        {100, 100, 100, 100, 100}, {0, 100, 100, 100, 100},
        {0, 62, 100, 100, 100}, {0, 50, 70, 100, 100},
        {0, 35, 55, 75, 100}, {0, 25, 50, 75, 100}
    };
    std::vector<int> out(2, 0);

    for (int period = 0; period < PERIODS; period++) {
        for (int set = 0; set < quantity[period]; set++) {
            int idx;
            do {
                int roll = rnd(100);
                auto it = std::find_if(std::begin(pct[period]), std::end(pct[period]),
                [=](char c) { return c > roll; });
                idx = std::distance(std::begin(pct[period]), it);
            } while (pool[idx].empty());
            int c = rnd(pool[idx].size());
            out.push_back(pool[idx][c]);
            pool[idx].erase(pool[idx].begin() + c);
        }
    }

    out.resize(cards);
    return out;
}

}  // namespace


BOOST_AUTO_TEST_SUITE(timeline_suite)

BOOST_AUTO_TEST_CASE(turn_index_helpers)
{
    for (int start : {55, 56, 57}) {
        auto p = NewPlayers(start, 63);
        BOOST_TEST(p->TurnIndex() == 0);
        BOOST_TEST(p->YearIndex() == 0);
        BOOST_TEST(p->BudgetIndex() == BUDGET_HISTORY_PAD);
        BOOST_TEST(p->IsFirstTurn());
        BOOST_TEST(p->BeforeFirstIntel());
        BOOST_TEST(!p->IsLastTurn());
        BOOST_TEST(p->TotalTurns() == 2 * (63 - start + 1));

        p->Season = 1;
        BOOST_TEST(p->TurnIndex() == 1);
        BOOST_TEST(!p->IsFirstTurn());
        BOOST_TEST(p->BeforeFirstIntel());

        p->Year = start + 1;
        p->Season = 0;
        BOOST_TEST(p->TurnIndex() == 2);
        BOOST_TEST(p->YearIndex() == 1);
        BOOST_TEST(p->BeforeFirstIntel());  // first briefing is in Fall

        p->Season = 1;
        BOOST_TEST(p->TurnIndex() == 3);
        BOOST_TEST(!p->BeforeFirstIntel());

        p->Year = 63;
        BOOST_TEST(p->IsLastTurn());
        BOOST_TEST(p->TurnIndex() == p->TotalTurns() - 1);
        p->Season = 0;
        BOOST_TEST(!p->IsLastTurn());
    }

    // The original game: turn 0 is Spring 1957, budget slot Year - 53.
    auto p = NewPlayers(57, 63);
    p->Year = 60;
    p->Season = 1;
    BOOST_TEST(p->TurnIndex() == 2 * (60 - 57) + 1);
    BOOST_TEST(p->BudgetIndex() == 60 - 53);
}

BOOST_AUTO_TEST_CASE(arrays_fit_the_longest_game)
{
    // 1955 to 1999
    auto p = NewPlayers(55, 99);
    BOOST_TEST(p->TotalTurns() <= MAX_GAME_TURNS);
    BOOST_TEST(p->TotalTurns() <= MAXIMUM_NEWS_EVENTS / 2);
    p->Year = 99;
    p->Season = 1;
    BOOST_TEST(p->BudgetIndex() < MAX_BUDGET_HISTORY);
    BOOST_TEST(p->YearIndex() < static_cast<int>(sizeof(p->P[0].PastIntel) /
                                                 sizeof(p->P[0].PastIntel[0])));
    BOOST_TEST(2 * p->TurnIndex() + 1 < MAXIMUM_NEWS_EVENTS);
    BOOST_TEST(p->P[0].eCount == 0);
    p->P[0].eCount = p->TotalTurns();
    BOOST_TEST(p->P[0].eCount <= MAX_NEWS_PER_PLAYER);
    BOOST_TEST(MAX_NEWS_ITEMS == 2 * MAX_NEWS_PER_PLAYER);
}

BOOST_AUTO_TEST_CASE(launch_windows_match_old_table_until_1963)
{
    for (int y = 55; y <= 63; y++) {
        for (int s = 0; s < 2; s++) {
            BOOST_TEST(InLaunchWindow(MarsWindows, y, s) == OldMars(y, s), "Mars " << y << "/" << s);
            BOOST_TEST(InLaunchWindow(JupiterWindows, y, s) == OldJupiter(y, s), "Jupiter " << y << "/" << s);
            BOOST_TEST(InLaunchWindow(SaturnWindows, y, s) == OldSaturn(y, s), "Saturn " << y << "/" << s);
        }
    }

    // The old table has no windows before 1960; so has the model.
    for (int y = 0; y < 60; y++) {
        BOOST_TEST(!InLaunchWindow(MarsWindows, y, 0));
        BOOST_TEST(!InLaunchWindow(MarsWindows, y, 1));
        BOOST_TEST(!InLaunchWindow(JupiterWindows, y, 0));
    }
}

BOOST_AUTO_TEST_CASE(launch_windows_deviations_from_old_table)
{
    // Known deviations of the synodic model from the hand-made table
    // for 1964-1977 (as turn = 2 * year + season).  Mars: the old table
    // jumps by 3.5 years between 1966 and 1969, a strictly periodic
    // model cannot.
    // Mars: the model has windows in the Fall of 68, 70, 72, 74 and the
    // Spring of 77; the old table in the Fall of 69, 71, 73 (8 turns).
    BOOST_TEST((Deviations(MarsWindows, OldMars) ==
                std::vector<int>{2 * 68 + 1, 2 * 69 + 1, 2 * 70 + 1, 2 * 71 + 1,
                                 2 * 72 + 1, 2 * 73 + 1, 2 * 74 + 1, 2 * 77}),
               "Mars deviations differ");
    // Jupiter: the old table has 72 as an extra year, 68 shifted by a half year
    BOOST_TEST((Deviations(JupiterWindows, OldJupiter) ==
                std::vector<int>{2 * 68, 2 * 69, 2 * 72, 2 * 72 + 1}),
               "Jupiter deviations differ");
    // Saturn: 71 Fall - 72 Spring instead of all of 72; extra 76 Fall - 77 Spring
    BOOST_TEST((Deviations(SaturnWindows, OldSaturn) ==
                std::vector<int>{2 * 71 + 1, 2 * 72 + 1, 2 * 76 + 1, 2 * 77}),
               "Saturn deviations differ");
}

BOOST_AUTO_TEST_CASE(launch_windows_work_for_any_year)
{
    // Windows keep repeating after 1977 with the synodic period.
    int marsCount = 0, jupCount = 0, satCount = 0;

    for (int y = 78; y <= 99; y++) {
        for (int s = 0; s < 2; s++) {
            marsCount += InLaunchWindow(MarsWindows, y, s);
            jupCount += InLaunchWindow(JupiterWindows, y, s);
            satCount += InLaunchWindow(SaturnWindows, y, s);
        }
    }

    // 22 years: about 10 Mars windows of one turn, 5 Jupiter windows of
    // two turns, about 4 Saturn windows of two turns.
    BOOST_TEST(marsCount >= 9);
    BOOST_TEST(marsCount <= 11);
    BOOST_TEST(jupCount >= 8);
    BOOST_TEST(jupCount <= 12);
    BOOST_TEST(satCount >= 6);
    BOOST_TEST(satCount <= 10);

    // MissionTimingOk goes through the model.
    BOOST_TEST(MissionTimingOk(Mission_MarsFlyby, 60, 0));
    BOOST_TEST(!MissionTimingOk(Mission_MarsFlyby, 61, 0));
    BOOST_TEST(MissionTimingOk(Mission_Orbital_Satellite, 55, 0));
}

BOOST_AUTO_TEST_CASE(event_cards_match_original_for_the_default_game)
{
    for (int seed : {1, 2, 3, 42}) {
        std::mt19937 gen1(seed), gen2(seed);
        auto r1 = [&](int n) { return static_cast<int>(gen1() % n); };
        auto r2 = [&](int n) { return static_cast<int>(gen2() % n); };

        const int turns = 2 * (63 - 57 + 1);
        std::vector<int8_t> cards(MAXIMUM_NEWS_EVENTS, 0);
        FillEventCards(cards.data(), turns, r1);
        std::vector<int> ref = OriginalEventCards(r2, 2 * turns);

        for (int i = 0; i < 2 * turns; i++) {
            BOOST_TEST(cards[i] == ref[i], "seed " << seed << " card " << i);
        }
    }
}

BOOST_AUTO_TEST_CASE(event_cards_for_all_game_lengths)
{
    for (int years = 7; years <= 45; years++) {
        std::mt19937 gen(years);
        auto rnd = [&](int n) { return static_cast<int>(gen() % n); };
        const int turns = 2 * years;
        std::vector<int8_t> cards(MAXIMUM_NEWS_EVENTS + 4, 99);
        FillEventCards(cards.data(), turns, rnd);

        // No empty card after the opening turn, none past the end.
        BOOST_TEST(cards[0] == 0);
        BOOST_TEST(cards[1] == 0);
        std::set<int> distinct;
        for (int i = 2; i < 2 * turns; i++) {
            BOOST_TEST((cards[i] >= 1 && cards[i] <= 99), "years " << years << " card " << i);
            distinct.insert(cards[i]);
        }

        // Fully filled: written up to 2 * turns, rest untouched.
        for (int i = 2 * turns; i < static_cast<int>(cards.size()); i++) {
            BOOST_TEST(cards[i] == 99, "overflow at " << i << " years " << years);
        }

        // Without reshuffling (up to 98 cards) every card is used once.
        if (2 * turns - 2 <= 98) {
            BOOST_TEST(static_cast<int>(distinct.size()) == 2 * turns - 2);
        }
    }

    // First cards come from the calm pool, as in the original.
    std::mt19937 gen(7);
    auto rnd = [&](int n) { return static_cast<int>(gen() % n); };
    std::vector<int8_t> cards(MAXIMUM_NEWS_EVENTS, 0);
    FillEventCards(cards.data(), 14, rnd);
    BOOST_TEST((cards[2] >= 1 && cards[2] <= 3));
    BOOST_TEST((cards[3] >= 1 && cards[3] <= 3));
    BOOST_TEST(cards[4] >= 4);
}

BOOST_AUTO_TEST_CASE(year_formatting)
{
    BOOST_TEST(FormatYear(55) == "1955");
    BOOST_TEST(FormatYear(57) == "1957");
    BOOST_TEST(FormatYear(63) == "1963");
    BOOST_TEST(FormatYear(99) == "1999");
    BOOST_TEST(FormatYear(100) == "2000");
    BOOST_TEST(FormatYear(105) == "2005");
    BOOST_TEST(FormatYear(127) == "2027");
    BOOST_TEST(FormatSeasonYear(57, 0) == "SPRING 1957");
    BOOST_TEST(FormatSeasonYear(100, 1) == "FALL 2000");
}

BOOST_AUTO_TEST_CASE(news_index_mapping)
{
    // 1955 and 1956 have no news.json text: use the 1957 block.
    BOOST_TEST(NewsYearIndex(55) == 0);
    BOOST_TEST(NewsYearIndex(56) == 0);
    BOOST_TEST(NewsYearIndex(57) == 0);
    BOOST_TEST(NewsYearIndex(58) == 1);
    BOOST_TEST(NewsYearIndex(63) == 6);

    // The US index (6 entries a year after the header entry) is
    // always at least 1, whatever the start year.
    for (int y = 55; y <= 63; y++) {
        for (int s = 0; s < 2; s++) {
            int us = NewsYearIndex(y) * 6 + s * 3 + 0 + 1;
            int su = NewsYearIndex(y) * 4 + s * 2 + 0 + 1;
            BOOST_TEST(us >= 1);
            BOOST_TEST(su >= 1);
        }
    }
}

BOOST_AUTO_TEST_CASE(urast_has_default_timeline)
{
    std::unique_ptr<Players> p(new Players());
    std::memset(p.get(), 0, sizeof(Players));
    std::string path = locate_file("urast.json", FT_DATA);
    BOOST_REQUIRE(!path.empty());
    DESERIALIZE_JSON_FILE(p, path);

    BOOST_TEST(p->Checksum == sizeof(Players));
    BOOST_TEST(p->StartYear == 57);
    BOOST_TEST(p->EndYear == 63);
    BOOST_TEST(p->Year == 57);
    BOOST_TEST(p->Season == 0);
}

BOOST_AUTO_TEST_CASE(save_game_round_trip_keeps_timeline)
{
    std::unique_ptr<Players> p(new Players());
    std::memset(p.get(), 0, sizeof(Players));
    std::string path = locate_file("urast.json", FT_DATA);
    DESERIALIZE_JSON_FILE(p, path);

    p->StartYear = 55;
    p->EndYear = 99;
    p->Year = 80;
    p->Season = 1;
    p->Count = 150;  // more than a signed byte can hold
    p->P[0].eCount = 51;
    p->P[1].BudgetHistory[MAX_BUDGET_HISTORY - 1] = 77;
    p->P[0].PastIntel[MAX_GAME_YEARS - 1].num = 5;
    p->P[0].History[MAX_MISSION_COUNT - 1].MissionYear = 99;
    p->Prestige[3].Index = 250;  // history index > 127

    std::stringstream ss;
    {
        cereal::JSONOutputArchive oar(ss);
        oar(cereal::make_nvp("Data", *p));
    }

    std::unique_ptr<Players> q(new Players());
    std::memset(q.get(), 0, sizeof(Players));
    {
        cereal::JSONInputArchive iar(ss);
        iar(cereal::make_nvp("Data", *q));
    }

    BOOST_TEST(q->StartYear == 55);
    BOOST_TEST(q->EndYear == 99);
    BOOST_TEST(q->Year == 80);
    BOOST_TEST(q->Season == 1);
    BOOST_TEST(q->Count == 150);
    BOOST_TEST(q->P[0].eCount == 51);
    BOOST_TEST(q->P[1].BudgetHistory[MAX_BUDGET_HISTORY - 1] == 77);
    BOOST_TEST(q->P[0].PastIntel[MAX_GAME_YEARS - 1].num == 5);
    BOOST_TEST(q->P[0].History[MAX_MISSION_COUNT - 1].MissionYear == 99);
    BOOST_TEST(q->Prestige[3].Index == 250);
}

BOOST_AUTO_TEST_SUITE_END()
