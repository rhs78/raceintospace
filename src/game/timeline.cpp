/*
 * Timeline helpers, see timeline.h.
 */
#include "timeline.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "draw.h"

std::string FormatYear(int year)
{
    return std::to_string(1900 + year);
}

std::string FormatSeasonYear(int year, int season)
{
    return std::string(season == 0 ? "SPRING " : "FALL ") + FormatYear(year);
}

void draw_year(int x, int y, int year)
{
    draw_string(x, y, FormatYear(year).c_str());
}

int NewsYearIndex(int year)
{
    return std::max(0, year - NEWS_BASE_YEAR);
}

int GameTurns(int startYear, int endYear)
{
    return 2 * (endYear - startYear + 1);
}

void FillEventCards(int8_t* cards, int turns,
                    const std::function<int(int)>& rnd)
{
    constexpr int NUM_POOLS = 5;
    constexpr int NUM_PERIODS = 6;
    // First card number and size of each pool.
    const int poolFirst[NUM_POOLS] = {1, 4, 44, 63, 80};
    const int poolSize[NUM_POOLS] = {3, 40, 19, 17, 20};
    // Number of cards in each period; the last one is the open ended one.
    const int quantityInSet[NUM_PERIODS] = {2, 8, 8, 12, 16, 1 << 20};
    const int poolPercentages[NUM_PERIODS][NUM_POOLS] = {
        {100, 100, 100, 100, 100},
        {  0, 100, 100, 100, 100},
        {  0,  62, 100, 100, 100},
        {  0,  50,  70, 100, 100},
        {  0,  35,  55,  75, 100},
        {  0,  25,  50,  75, 100}
    };

    const int total = 2 * turns;
    for (int i = 0; i < total; i++) {
        cards[i] = 0;
    }

    std::vector<std::vector<int>> pools(NUM_POOLS);
    auto refill = [&]() {
        for (int p = 0; p < NUM_POOLS; p++) {
            pools[p].resize(poolSize[p]);
            std::iota(pools[p].begin(), pools[p].end(), poolFirst[p]);
        }
    };
    refill();

    int cardCount = 2;  // the first turn has no events
    for (int period = 0; period < NUM_PERIODS && cardCount < total; period++) {
        const int* dist = poolPercentages[period];
        for (int set = 0; set < quantityInSet[period] && cardCount < total; set++) {
            // Pools that may be used in this period (nonzero share).
            // When they are all empty, reshuffle.
            bool usable = false;
            for (int p = 0; p < NUM_POOLS; p++) {
                int share = dist[p] - (p > 0 ? dist[p - 1] : 0);
                if (share > 0 && !pools[p].empty()) {
                    usable = true;
                }
            }
            if (!usable) {
                refill();
            }

            int pool_idx;
            do {
                // Pick a pool: first pool whose cumulative share exceeds the roll.
                int roll = rnd(100);
                pool_idx = 0;
                while (pool_idx < NUM_POOLS - 1 && dist[pool_idx] <= roll) {
                    pool_idx++;
                }
            } while (pools[pool_idx].empty());

            auto& pool = pools[pool_idx];
            int idx = rnd(static_cast<int>(pool.size()));
            cards[cardCount++] = static_cast<int8_t>(pool[idx]);
            pool.erase(pool.begin() + idx);
        }
    }
}

// Launch windows.
//
// The planets' launch windows repeat with their synodic period
// (Mars 779.9 days, Jupiter 398.9 days, Saturn 378.1 days).  The
// original game used a hand-made table for 1957-1977.  The model below
// is fitted to that table: it reproduces it exactly up to and including
// 1963 (which covers the whole default game) and differs for some turns
// afterwards, see test/game/timeline_test.cpp.
//
// Windows only exist from the epoch onwards: the original table has no
// window before 1960/61 (no hardware could use one).
//  - Mars: every synodic period, window of one turn.
//  - Jupiter: Jupiter flybys are only possible roughly every fourth
//    synodic period (the cheap Earth-Jupiter geometry), window of a year.
//  - Saturn: every fifth synodic period, window of a year.
const LaunchWindowModel MarsWindows = {60.0, 2.13540, 1};
const LaunchWindowModel JupiterWindows = {60.0, 4 * 1.09207, 2};
const LaunchWindowModel SaturnWindows = {61.2, 5 * 1.03519, 2};

bool InLaunchWindow(const LaunchWindowModel& m, int year, int season)
{
    const long turn = 2L * year + season;
    // Window k starts in turn floor(2 * (epoch + k * period)).
    long k0 = static_cast<long>(std::floor((turn / 2.0 - m.epoch) / m.period));

    for (long k = std::max(0L, k0 - 1); k <= std::max(0L, k0 + 1); k++) {
        long start = static_cast<long>(std::floor(2.0 * (m.epoch + k * m.period)));
        if (turn >= start && turn < start + m.lengthTurns) {
            return true;
        }
    }

    return false;
}
