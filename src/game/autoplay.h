#ifndef AUTOPLAY_H
#define AUTOPLAY_H

/**
 * \file autoplay.h
 *
 * Headless autoplay mode (doc/fork/autoplay.md): two AI players play a
 * whole game without human input.  The mode is switched on with
 * `--autoplay` (options.autoplay); every blocking UI wait in sdlhelper.cpp /
 * pace.cpp then returns at once (see av_block()).
 */

#include <string>

/** Settings of an autoplay run (command line, see setup_options()). */
struct AutoplayConfig {
    unsigned seed = 1;        ///< --seed N: seed of the random number generator
    int startYear = 0;        ///< --start-year 55|56|57 (0: scenario default)
    int model = 0;            ///< --model: 0 basic, 1 historical, 2 random
    bool sandbox = false;     ///< --sandbox
    std::string scenario;     ///< --scenario id ("" = default scenario)
    int years = 0;            ///< --years N: play only N game years (0: to the end)
};

extern AutoplayConfig autoplayConfig;

/** printf-like log line "[autoplay] ..." on stdout, flushed at once. */
void AutoplayLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/** Name of a start model number of AutoplayConfig::model; nullptr if invalid. */
const char* AutoplayModelName(int model);

/**
 * Set up and run a complete AI vs. AI game (used instead of the main menu).
 * Returns when the game has ended.
 */
void AutoplayNewGame();

/** Log the state at the start of a turn (year, season, cash, prestige). */
void AutoplayLogTurn();

/** Log the end state (winner by prestige / lunar landing). */
void AutoplayLogEnd();

#endif // AUTOPLAY_H
