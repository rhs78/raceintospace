/*
 * Timeline helpers: game length, year formatting, news indexing and
 * interplanetary launch windows.
 *
 * Years are always "years since 1900", like Players::Year.
 */
#ifndef TIMELINE_H
#define TIMELINE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/** First calendar year the news.json texts are written for (1957). */
constexpr int NEWS_BASE_YEAR = 57;

/**
 * Four digit calendar year ("1957", "2003") for a year given as
 * years since 1900.  Works for years >= 100.
 */
std::string FormatYear(int year);

/** "SPRING 1957" / "FALL 1957". */
std::string FormatSeasonYear(int year, int season);

/**
 * Draw the four digit year at the current cursor position (or at x, y
 * when both are non-zero), see draw_string().
 */
void draw_year(int x, int y, int year);

/**
 * Index of the news.json year block for the given game year.
 * Years before the first year for which news texts exist are mapped
 * to the first block (1957).
 */
int NewsYearIndex(int year);

/**
 * Number of (spring and fall) turns of a game running from startYear
 * to endYear, both inclusive.
 */
int GameTurns(int startYear, int endYear);

/**
 * Fill the event card table.
 *
 * Event cards are drawn per game turn (two per turn, one per player;
 * cards[0] and cards[1] stay 0 for the opening turn).  The original game
 * hand-picked the cards for 21 years: the first 2 cards from the
 * calmest pool, the next 8 and 8 from mostly calm pools, then 12, 16
 * and finally 52 cards with a growing share of harsh events.  The
 * same period structure is used for every game length, counted from
 * the first turn of the game; the final period is stretched (or
 * truncated) to the number of cards the game needs.  When all cards
 * of the pools allowed for a period are used up the pools are
 * reshuffled, so very long games never run out of cards.
 *
 * \param cards  table to fill, at least 2 * turns entries.
 * \param turns  number of turns of the game.
 * \param rnd  random number source; rnd(n) returns a value in [0, n).
 */
void FillEventCards(int8_t* cards, int turns,
                    const std::function<int(int)>& rnd);

/** Interplanetary launch window model: synodic period based. */
struct LaunchWindowModel {
    double epoch;      /**< start of the first window (years since 1900) */
    double period;     /**< distance between windows in years */
    int lengthTurns;   /**< window length in turns (half years) */
};

/** The model used for a mission, see mission_util.cpp. */
extern const LaunchWindowModel MarsWindows;
extern const LaunchWindowModel JupiterWindows;
extern const LaunchWindowModel SaturnWindows;

/** Is the given turn inside one of the launch windows of the model? */
bool InLaunchWindow(const LaunchWindowModel& model, int year, int season);

#endif // TIMELINE_H
