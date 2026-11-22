#ifndef SCENARIO_H
#define SCENARIO_H

/**
 * \file scenario.h
 *
 * Scenarios (stage 4c).  A scenario lists the hardware programs of each
 * side (catalog ids in order), the default start and end year and the start
 * values of each program for the three start models (basic, historical,
 * random).  The initial game state is built from a scenario and the hardware
 * catalog.  See the files in data/gamedata/scenarios and doc/fork/hardware-katalog.md.
 */

#include <map>
#include <string>
#include <vector>

#include "data.h"
#include "hardware_catalog.h"

/** Start model of the equipment values (Defaults.Input 0/1, 2/3, 4/5). */
enum class StartModel {
    Basic,       ///< values of the basic model
    Historical,  ///< values of the historical model
    Random       ///< basic model, then randomized by the scenario's rules
};

/** The start values of a program for one start model. */
struct StartStats {
    int safety = 0;
    int base = -1;           ///< -1: same as safety
    int initCost = 0;
    int unitCost = 0;
    int unitWeight = 0;
    int maxPay = 0;
    int rdCost = 0;
    int maxRD = 0;
    int maxSafety = 0;
    int duration = 0;
    int num = -1;            ///< PROGRAM_NOT_STARTED
};

/** Expression of the random model: a number, an equipment value or a maximum. */
struct RandomExpr {
    enum class Kind { Number, Self, Ref, Max };

    Kind kind = Kind::Number;
    double number = 0;
    std::string field;              ///< Self/Ref: name of the value
    HardwareRole role = HardwareRole::Launcher;  ///< Ref: role of the program
    int ordinal = 0;                ///< Ref: n-th program with this role
    bool hasMul = false;
    double mul = 1.0;
    int div = 0;                    ///< 0: none
    int add = 0;
    std::vector<RandomExpr> args;   ///< Max
};

/** One value of a program that the random model sets. */
struct RandomOp {
    enum class Fn { Number, Min, Const };

    std::string field;
    Fn fn = Fn::Const;
    std::vector<RandomExpr> args;
    bool hasBase = false;
    RandomExpr base;
    bool negate = false;
    bool hasMaxValue = false;
    int maxValue = 0;
};

/** The random model's rules for one program, selected by role and ordinal. */
struct RandomRule {
    HardwareRole role = HardwareRole::Launcher;
    int ordinal = 0;
    std::vector<RandomOp> ops;
};

class Scenario
{
public:
    /**
     * Parse and validate a scenario document.
     *
     * \param text  the JSON text.
     * \param catalog  the catalog the ids refer to.
     * \param parent  the scenario named by "extends" (nullptr if none).
     * \throws std::runtime_error  on parse or validation errors.
     */
    static Scenario FromJsonText(const std::string& text,
                                 const HardwareCatalog& catalog,
                                 const Scenario* parent = nullptr);

    /** Name of the scenario this one extends, found in a JSON text; "". */
    static std::string ParentIdOf(const std::string& text);

    const std::string& id() const { return mId; }
    const std::string& name() const { return mName; }
    const std::string& description() const { return mDescription; }
    bool hidden() const { return mHidden; }
    int startYear() const { return mStartYear; }
    int endYear() const { return mEndYear; }

    /** Catalog indices of a side's category list, in order. */
    const std::vector<int>& list(int player, int category) const
    {
        return mLists[player][category];
    }

    /** Largest list size of the two sides for a category. */
    int count(int category) const;

    /** Start values; the basic ones if the model has none for the program. */
    const StartStats* stats(StartModel model, int catalogIndex) const;

    const std::vector<RandomRule>& randomRules() const { return mRandom; }

private:
    std::string mId;
    std::string mName;
    std::string mDescription;
    bool mHidden = false;
    int mStartYear = MIN_START_YEAR;
    int mEndYear = MIN_START_YEAR + 6;
    std::vector<int> mLists[NUM_PLAYERS][4];
    std::map<int, StartStats> mBasic;
    std::map<int, StartStats> mHistorical;
    std::vector<RandomRule> mRandom;
};

/** Id of the scenario used when none is selected. */
const char* DefaultScenarioId();

/**
 * Load (once) the scenario with the given id (case-insensitive) from
 * data/gamedata/scenarios/<id>.json.
 * \throws std::runtime_error  if it is missing or invalid.
 */
const Scenario& GetScenario(const std::string& id);

/** Ids of the available scenarios (hidden ones only if requested). */
std::vector<std::string> ListScenarios(bool includeHidden = false);

/**
 * Build the hardware part of the game state of a scenario: the four
 * hardware lists of both players (and the tables sized by them), the
 * scenario id, and the scenario's default start and end year.
 *
 * \param data  the game state; its non-hardware members are untouched.
 */
void BuildHardware(Players& data, const Scenario& scenario, StartModel model);

/** Set the values of a model on the hardware lists of data (same lists). */
void ApplyStartModel(Players& data, const Scenario& scenario,
                     StartModel model);

/**
 * Start state of a new game: the non-hardware initial state
 * (data/gamedata/urast.json) plus the hardware of the scenario.
 *
 * \throws std::runtime_error  if a file is missing or invalid.
 */
void BuildStartState(Players& data, const std::string& scenarioId,
                     StartModel model);

/** The scenario of a game state (data.Scenario, else the default). */
const Scenario& ScenarioOf(const Players& data);

/**
 * Re-randomize the equipment values of the game state with the
 * scenario's random model (RandomizeEq).
 */
void ApplyRandomModel(Players& data, const Scenario& scenario);

#endif // SCENARIO_H
