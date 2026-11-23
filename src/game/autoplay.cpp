/**
 * \file autoplay.cpp
 *
 * Headless autoplay mode: a complete AI vs. AI game without human input.
 * See doc/fork/autoplay.md.
 */

#include "autoplay.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <exception>
#include <stdexcept>

#include "admin.h"
#include "data.h"
#include "endgame.h"
#include "game_main.h"
#include "hardware.h"
#include "options.h"
#include "pbm.h"
#include "randomize.h"
#include "records.h"
#include "scenario.h"
#include "state_utils.h"

extern Players* Data;
extern char plr[NUM_PLAYERS];
extern char AI[2];
extern unsigned char LOAD;
extern unsigned char QUIT;
extern char Option;
void InitData();
void MainLoop();

AutoplayConfig autoplayConfig;

void AutoplayLog(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[autoplay] ", stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
    fflush(stdout);
}

const char* AutoplayModelName(int model)
{
    switch (model) {
    case 0: return "basic";
    case 1: return "historical";
    case 2: return "random";
    default: return nullptr;
    }
}

namespace
{
const char* SeasonName(int season)
{
    return season == 0 ? "Spring" : "Fall";
}
}

void AutoplayNewGame()
{
    const AutoplayConfig& cfg = autoplayConfig;

    // Deterministic from here on: every brandom()/rand() call derives
    // from the seed.
    srand(cfg.seed);

    MakeRecords();

    const std::string scenario =
        cfg.scenario.empty() ? std::string(DefaultScenarioId()) : cfg.scenario;
    const StartModel model = cfg.model == 1 ? StartModel::Historical
                             : StartModel::Basic;

    try {
        BuildStartState(*Data, scenario, model);
    } catch (const std::exception& e) {
        fprintf(stderr, "autoplay: cannot build the start state of scenario "
                "'%s': %s\n", scenario.c_str(), e.what());
        exit(2);
    }

    // Same sequence as the preferences screen (prefs.cpp), with both
    // sides computer controlled.  Input: 0 basic, 2 historical, 4 random
    // (even numbers: historical crews).
    Data->Def.Plr1 = 2;  // AI + USA
    Data->Def.Plr2 = 3;  // AI + USSR
    Data->Def.Lev1 = cfg.seed % 3;
    Data->Def.Lev2 = (cfg.seed / 3) % 3;
    Data->Def.Ast1 = Data->Def.Ast2 = 0;
    Data->Def.Input = 2 * cfg.model;
    Data->Def.Sandbox = cfg.sandbox ? 1 : 0;

    if (cfg.startYear != 0) {
        Data->StartYear = cfg.startYear;
    }

    if (cfg.years > 0) {
        Data->EndYear = Data->StartYear + cfg.years - 1;
        if (Data->EndYear - Data->StartYear >= MAX_GAME_YEARS) {
            Data->EndYear = Data->StartYear + MAX_GAME_YEARS - 1;
        }
    }

    if (Data->EndYear < Data->StartYear) {
        fprintf(stderr, "autoplay: end year %d before start year %d\n",
                Data->EndYear, Data->StartYear);
        exit(2);
    }

    if (cfg.model == 2) {
        RandomizeEq();
    }

    for (int i = 0; i < NUM_PLAYERS; i++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            for (Equipment& e : HardwareList(Data->P[i], c)) {
                e.MSF = e.MaxRD;
            }
        }
    }

    CacheCrewFile();

    LOAD = QUIT = 0;
    MAIL = -1;
    Option = -1;

    plr[0] = Data->Def.Plr1;
    plr[1] = Data->Def.Plr2;
    Data->plr[0] = Data->Def.Plr1;
    Data->plr[1] = Data->Def.Plr2;
    AI[0] = AI[1] = 1;

    Data->Year = Data->StartYear;
    Data->Season = 0;

    AutoplayLog("seed=%u scenario=%s model=%s sandbox=%d start=19%d end=19%d "
                "levels=%d/%d", cfg.seed, scenario.c_str(),
                AutoplayModelName(cfg.model), (int) Data->Def.Sandbox,
                Data->StartYear, Data->EndYear, (int) Data->Def.Lev1,
                (int) Data->Def.Lev2);

    InitData();
    MainLoop();
    AutoplayLogEnd();
}

void AutoplayEquipExtras(int plr)
{
    if (!options.autoplay) {
        return;
    }

    for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
        for (Equipment& e : HardwareList(Data->P[plr], c)) {
            const HardwareEntry* entry = HardwareInfo(e);

            if (entry == nullptr || !entry->testOnly) {
                continue;
            }

            // Better than every classic program, so that the AI's choice
            // (the safest usable one) picks the test hardware.
            e.Num = std::max<int>(e.Num, 2);
            e.MaxRD = std::max<int>(e.MaxRD, 99);
            e.MaxSafety = std::max<int>(e.MaxSafety, 99);
            e.Safety = std::max<int>(e.Safety, 99);
            e.Delay = 0;
        }
    }
}

std::string AutoplayHardwareSummary(int plr, const MissionType& mission)
{
    static const char* const slots[] = {"capsule", "kicker", "lm", "payload",
                                        "rocket", "eva"
                                       };
    std::string text;

    for (int i = Mission_Capsule; i <= Mission_EVA; i++) {
        const Equipment* e =
            MissionEquipment(plr, mission, static_cast<MissionHardwareType>(i));

        if (e == nullptr) {
            continue;
        }

        text += std::string(text.empty() ? "" : " ") + slots[i] + "=" + e->Name;

        if (i == Mission_Rocket && mission.Booster) {
            text += "+B";
        }
    }

    return text;
}

void AutoplayLogTurn()
{
    AutoplayLog("turn %d: 19%02d %s  cash USA=%d USSR=%d  budget USA=%d USSR=%d",
                Data->TurnIndex() + 1, Data->Year, SeasonName(Data->Season),
                (int) Data->P[0].Cash, (int) Data->P[1].Cash,
                (int) Data->P[0].Budget, (int) Data->P[1].Budget);
}

void AutoplayLogEnd()
{
    const PrestType& landing = Data->Prestige[Prestige_MannedLunarLanding];
    const int usa = PrestigeTotal(0);
    const int ussr = PrestigeTotal(1);
    const char* names[] = {"USA", "USSR"};

    if (landing.Place != -1) {
        AutoplayLog("END: manned lunar landing by %s in 19%d; points USA=%d USSR=%d",
                    names[(int) landing.Place & 1], Data->Year, usa, ussr);
    } else {
        const int win = FinalScoreWinner();
        AutoplayLog("END: no landing, winner on points: %s; points USA=%d USSR=%d",
                    win < 0 ? "none (tie)" : names[win], usa, ussr);
    }

    AutoplayLog("RESULT OK");
}
