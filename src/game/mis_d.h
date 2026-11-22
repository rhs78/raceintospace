/*
    Crisis system: player decisions during manned missions.

    Specification: doc/fork/krisensystem.md
    Data:          data/gamedata/decisions.json

    This module is split in two layers:
      - Pure logic (classification, probabilities, AI policy, alarm
        trigger, scenario lookup, data loading). No SDL, unit-tested in
        test/game/crisis_test.cpp.
      - A thin UI/resolution layer (OfferCrisis, OfferAlarm) which draws
        the dialog, rolls the dice and updates cash and per-mission state.
*/
#ifndef MIS_D_H
#define MIS_D_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "data.h"

/** How serious a step failure is, derived from the fail code. */
enum class CrisisClass {
    Minor,          /**< no dialog, original behaviour */
    MissionThreat,  /**< mission objective in danger */
    LifeThreat      /**< crew in danger */
};

enum class AlarmOption { Go, Abort };

/** Qualitative display of a probability. */
enum class Outlook { Poor, Fair, Good };

/** What happened in a crisis dialog, as seen by MisCheck(). */
enum class CrisisOutcome {
    None,         /**< no dialog took place; apply the failure as usual */
    Accepted,     /**< player accepts the consequences (original FailEval) */
    AbortOk,      /**< safe abort: partial failure, crew alive */
    AbortFailed,  /**< abort attempt failed: original FailEval */
    FixOk,        /**< ground team solved it: step counts as success */
    FixFailed,    /**< ground team failed: original FailEval */
    Continued     /**< continue at risk: step failed but mission goes on */
};

/**
 * New StepInfo codes. All lie in the "PARTIAL FAILURE" range (500-1998)
 * so MaxFail(), the mission review (place.cpp) and history treat them
 * as a partial failure without further changes.
 */
enum CrisisStepInfo {
    STEPINFO_ABORT = 1960,        /**< crisis abort, mission-class failure */
    STEPINFO_ABORT_LIFE = 1961,   /**< crisis abort, crew saved (life threat) */
    STEPINFO_ALARM_ABORT = 1962,  /**< abort after alarm */
    STEPINFO_CONTINUED = 1963     /**< failure accepted, mission continues */
};

/**
 * Marker stored in MisEval::rnum (a step's failure roll, which is dead
 * data once the step has been resolved; legal rolls are -5..10000) when
 * the ground team rescued a life-threatening failure. AllotPrest() finds
 * it again from the stored Mev[] data, so PBEM replays award the same
 * prestige without re-running the dialog.
 */
const int CRISIS_RESCUED_LIFE_MARK = 20001;

struct DecisionParams {
    int abortSafetyDivisor = 4;
    int abortMin = 5;
    int abortMax = 95;

    int groundCostLife = 10;      /**< MB */
    int groundCostMission = 5;    /**< MB */
    int groundProgressDivisor = 5;
    int groundPenaltyLife = 15;
    int groundPenaltyMission = 5;
    int groundMin = 5;
    int groundMax = 95;

    int continueMalus = 10;
    bool escalateAfterContinue = true;

    int hintReliability = 70;
    int hintMin = 50;
    int hintMax = 95;

    int alarmBand = 3;
    int alarmChance = 30;
    std::string alarmSteps = "ADIS";  /**< step letters (A=0, B=1, ...) */

    int aiAlarmGoSafety = 75;

    int prestigeSavedCrew = 2;
    int prestigeAlarmAbort = -1;

    int outlookGood = 60;
    int outlookFair = 35;
};

struct ScenarioText {
    std::string title;
    std::string text;
};

struct DecisionScenario {
    std::string id;
    std::string kind;                  /**< "crisis" or "alarm" */
    std::vector<std::string> fnames;   /**< failure groups; '?' matches any char */
    std::vector<int> codes;            /**< optional fail codes; empty = any */
    std::vector<std::string> classes;  /**< optional "life"/"mission"; empty = any */
    ScenarioText text;
};

/**
 * Parameters of one rescue system (catalog field abort_system), from
 * decisions.json "abort_systems".
 */
struct AbortSystemParams {
    int launchBase = -1;       /**< replaces the abort base at launch; -1 = keep */
    int landingBase = -1;      /**< replaces the abort base at the lunar landing; -1 = keep */
    int injuryChance = 0;      /**< percent per crew member on a successful abort */
    bool availableAtLaunch = true;
    std::string label;         /**< shown as "ABORT: <label>" */
};

struct DecisionData {
    DecisionParams params;
    /** By catalog abort_system value (escape_tower, ejection_seats, ...). */
    std::map<std::string, AbortSystemParams> abortSystems;
    int abortBaseDefault = 40;
    std::map<char, int> abortBase;     /**< by step letter */
    ScenarioText genericLife;
    ScenarioText genericMission;
    ScenarioText genericAlarm;
    std::vector<DecisionScenario> scenarios;
};

// ---------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------

/** Built-in defaults (the same values as shipped in decisions.json). */
DecisionData DefaultDecisionData();

/**
 * Parse decisions.json content. Missing keys keep their defaults.
 * \return false (and set *error) if the text is not valid JSON.
 */
bool ParseDecisionData(const std::string& json, DecisionData& out,
                       std::string* error = nullptr);

/** Lazily loaded decisions.json; falls back to defaults with a warning. */
const DecisionData& GetDecisionData();

/** Drop the cached data (tests). */
void ResetDecisionData();

// ---------------------------------------------------------------------
// Pure logic
// ---------------------------------------------------------------------

CrisisClass ClassifyFailure(int failCode);
CrisisClass Escalate(CrisisClass c);

/** Step letter ('A'..) for a MisEval::loc value. */
char StepLetter(int loc);

/** Abort base chance (percent) for a step. */
int AbortBase(const DecisionData& d, int loc);

/** Percent chance of a successful abort. */
int AbortChance(const DecisionData& d, int loc, int hardwareSafety,
                int crewBonus);

/**
 * Rescue system rules. \p system is the catalog abort_system of the
 * vehicle that matters for the step ("" if unknown or not applicable).
 * The launch step (loc 0) uses the capsule's system, the lunar landing
 * (loc 18) the lander's "ascent_stage"; all other steps are unaffected.
 */
const AbortSystemParams* AbortSystemFor(const DecisionData& d, int loc,
                                        const std::string& system);

/** Is an abort possible at all (false for "none" at launch)? */
bool AbortAvailable(const DecisionData& d, int loc,
                    const std::string& system);

/** Abort base chance, honouring the rescue system. */
int AbortBase(const DecisionData& d, int loc, const std::string& system);

/** Percent chance of a successful abort, honouring the rescue system. */
int AbortChance(const DecisionData& d, int loc, const std::string& system,
                int hardwareSafety, int crewBonus);

/** "ABORT MISSION" or "ABORT: <SYSTEM LABEL>". */
std::string AbortLabel(const DecisionData& d, int loc,
                       const std::string& system);

/**
 * Which crew members are injured by a successful abort? Each of
 * \p crew members is injured independently with \p injuryChance percent.
 * \param roll100  returns a random number in 1..100
 */
std::vector<bool> RollAbortInjuries(int crew, int injuryChance,
                                    const std::function<int()>& roll100);

int GroundCost(const DecisionParams& p, CrisisClass c);

/** Bonus added to the ground team re-roll. */
int GroundBonus(const DecisionParams& p, int crewBonus, int hardwareSafety,
                int hardwareBase);

/** Percent chance that the ground team's re-roll succeeds. */
int GroundFixChance(const DecisionParams& p, CrisisClass c, int stepSafety,
                    int bonus);

/** Percent chance that the flight director's hint is right. */
int HintReliability(const DecisionParams& p, int crewBonus);

Outlook OutlookOf(const DecisionParams& p, int percent);
const char* OutlookName(Outlook o);

bool AlarmEligible(const DecisionParams& p, int loc);

/**
 * Does an alarm fire for this step?
 * \param roll  a random number in 1..100
 */
bool AlarmTriggered(const DecisionParams& p, int loc, int dice, int safety,
                    int roll);

/**
 * The flight director's recommendation.
 * \param problem  whether the roll actually is a failure
 * \param roll     a random number in 1..100
 */
AlarmOption AlarmHint(const DecisionParams& p, bool problem, int crewBonus,
                      int roll);

// AI policy (no dialog)
enum class CrisisChoice { Accept, Abort, GroundTeam, Continue };

/**
 * \param abortAvailable  false if the vehicle has no escape system for
 *                        this step: the AI then tries the ground team
 *                        and otherwise accepts the consequences.
 */
CrisisChoice AiCrisisChoice(const DecisionParams& p, CrisisClass c,
                            int cash, bool groundUsed,
                            bool abortAvailable = true);
AlarmOption AiAlarmChoice(const DecisionParams& p, int safety,
                          bool abortAvailable = true);

// Scenario lookup
bool FNameMatches(const std::string& pattern, const std::string& fname);

/**
 * Find the text for a dialog.
 * \param kind   "crisis" or "alarm"
 * \param fname  failure group, e.g. "F008"
 * \param code   fail code, or -1 if unknown (alarms)
 */
ScenarioText FindScenario(const DecisionData& d, const std::string& kind,
                          const std::string& fname, int code,
                          CrisisClass cls);

// ---------------------------------------------------------------------
// Per-mission state
// ---------------------------------------------------------------------

struct CrisisState {
    bool groundUsed = false;  /**< ground team used this mission */
    int malus = 0;            /**< safety malus on all later steps */
    bool escalate = false;    /**< next failure is classed one level worse */
};

CrisisState& CrisisMission();
void CrisisBeginMission();

/** Apply (and consume) a pending escalation. */
CrisisClass ApplyEscalation(CrisisState& s, CrisisClass c);

/**
 * Prestige adjustment from crisis decisions, read from stored Mev[]:
 * +prestigeSavedCrew for each saved crew, prestigeAlarmAbort for an
 * alarm abort. Walks the mission trace like AllotPrest().
 */
int CrisisPrestigeDelta();

// ---------------------------------------------------------------------
// Resolution + UI
// ---------------------------------------------------------------------

struct CrisisRequest {
    char plr = 0;
    const MisEval* step = nullptr;
    const XFails* fail = nullptr;
    CrisisClass cls = CrisisClass::Minor;  /**< after escalation */
    int safety = 0;           /**< StepSafety incl. malus */
    int hardwareSafety = 0;   /**< equipment MisSaf (without crew bonus) */
    int hardwareBase = 0;     /**< equipment Base safety */
    int hardwareCurrent = 0;  /**< equipment Safety */
    /** Catalog abort_system of the relevant capsule/lander ("" = none known). */
    std::string abortSystem;
};

/**
 * Offer the crisis options (dialog for humans, AI policy otherwise),
 * roll the outcome, pay the cost and update CrisisMission().
 */
CrisisOutcome OfferCrisis(const CrisisRequest& req);

/**
 * Offer GO / ABORT for an alarm.
 * \param problem  whether the (hidden) roll is a failure
 */
AlarmOption OfferAlarm(char plr, const MisEval& step, int dice, int safety,
                       bool problem, const std::string& abortSystem = "");

#endif // MIS_D_H
