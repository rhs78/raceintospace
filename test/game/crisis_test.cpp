#include <boost/test/unit_test.hpp>

#include <cstring>

#include "game/mc.h"
#include "game/mis_d.h"

BOOST_AUTO_TEST_SUITE(crisis_suite)

BOOST_AUTO_TEST_CASE(classification_matches_spec)
{
    for (int c : {3, 5, 13, 16, 17, 22, 31, 33}) {
        BOOST_TEST((ClassifyFailure(c) == CrisisClass::LifeThreat), "code " << c);
    }

    for (int c : {2, 4, 15, 26, 30}) {
        BOOST_TEST((ClassifyFailure(c) == CrisisClass::MissionThreat), "code " << c);
    }

    for (int c : {0, 6, 7, 9, 12, 18, 19, 20, 23, 24, 25, 40, 99, -1}) {
        BOOST_TEST((ClassifyFailure(c) == CrisisClass::Minor), "code " << c);
    }
}

BOOST_AUTO_TEST_CASE(escalation)
{
    BOOST_TEST((Escalate(CrisisClass::Minor) == CrisisClass::MissionThreat));
    BOOST_TEST((Escalate(CrisisClass::MissionThreat) == CrisisClass::LifeThreat));
    BOOST_TEST((Escalate(CrisisClass::LifeThreat) == CrisisClass::LifeThreat));

    CrisisState s;
    BOOST_TEST((ApplyEscalation(s, CrisisClass::MissionThreat) == CrisisClass::MissionThreat));
    s.escalate = true;
    BOOST_TEST((ApplyEscalation(s, CrisisClass::MissionThreat) == CrisisClass::LifeThreat));
    BOOST_TEST(!s.escalate);  // consumed
    BOOST_TEST((ApplyEscalation(s, CrisisClass::MissionThreat) == CrisisClass::MissionThreat));
}

BOOST_AUTO_TEST_CASE(step_letters)
{
    BOOST_TEST(StepLetter(0) == 'A');
    BOOST_TEST(StepLetter(3) == 'D');
    BOOST_TEST(StepLetter(8) == 'I');
    BOOST_TEST(StepLetter(18) == 'S');
    BOOST_TEST(StepLetter(25) == 'Z');
    BOOST_TEST(StepLetter(26) == 'a');
}

BOOST_AUTO_TEST_CASE(abort_chance_formula)
{
    DecisionData d = DefaultDecisionData();
    // base(S)=40, safety 80 / 4 = 20, crew +3
    BOOST_TEST(AbortChance(d, 18, 80, 3) == 63);
    // unknown step letter -> default base
    BOOST_TEST(AbortChance(d, 31, 80, 0) == d.abortBaseDefault + 20);
    // clamped
    BOOST_TEST(AbortChance(d, 5, 99, 50) == d.params.abortMax);
    BOOST_TEST(AbortChance(d, 3, 0, -50) == d.params.abortMin);
}

BOOST_AUTO_TEST_CASE(ground_team_formulas)
{
    DecisionParams p;
    BOOST_TEST(GroundCost(p, CrisisClass::LifeThreat) == 10);
    BOOST_TEST(GroundCost(p, CrisisClass::MissionThreat) == 5);

    // bonus = crew + (safety - base) / 5
    BOOST_TEST(GroundBonus(p, 2, 90, 60) == 2 + 6);
    BOOST_TEST(GroundBonus(p, 2, 50, 60) == 2);  // no negative progress

    // chance = safety + bonus - penalty (class), clamped
    BOOST_TEST(GroundFixChance(p, CrisisClass::LifeThreat, 70, 8)
               == 70 + 8 - p.groundPenaltyLife);
    BOOST_TEST(GroundFixChance(p, CrisisClass::MissionThreat, 70, 8)
               == 70 + 8 - p.groundPenaltyMission);
    BOOST_TEST(GroundFixChance(p, CrisisClass::MissionThreat, 99, 30) == p.groundMax);
    BOOST_TEST(GroundFixChance(p, CrisisClass::LifeThreat, 1, 0) == p.groundMin);
}

BOOST_AUTO_TEST_CASE(hint_reliability_and_hint)
{
    DecisionParams p;
    BOOST_TEST(HintReliability(p, 0) == 70);
    BOOST_TEST(HintReliability(p, 5) == 75);
    BOOST_TEST(HintReliability(p, 100) == p.hintMax);

    // reliable roll -> truth, unreliable roll -> the opposite
    BOOST_TEST((AlarmHint(p, true, 0, 1) == AlarmOption::Abort));
    BOOST_TEST((AlarmHint(p, false, 0, 1) == AlarmOption::Go));
    BOOST_TEST((AlarmHint(p, true, 0, 70) == AlarmOption::Abort));
    BOOST_TEST((AlarmHint(p, true, 0, 71) == AlarmOption::Go));
    BOOST_TEST((AlarmHint(p, false, 0, 100) == AlarmOption::Abort));
    // crew bonus widens the reliable range
    BOOST_TEST((AlarmHint(p, true, 5, 75) == AlarmOption::Abort));
}

BOOST_AUTO_TEST_CASE(outlook)
{
    DecisionParams p;
    BOOST_TEST((OutlookOf(p, 80) == Outlook::Good));
    BOOST_TEST((OutlookOf(p, 60) == Outlook::Good));
    BOOST_TEST((OutlookOf(p, 59) == Outlook::Fair));
    BOOST_TEST((OutlookOf(p, 35) == Outlook::Fair));
    BOOST_TEST((OutlookOf(p, 34) == Outlook::Poor));
    BOOST_TEST(std::string(OutlookName(Outlook::Good)) == "GOOD");
}

BOOST_AUTO_TEST_CASE(alarm_trigger)
{
    DecisionParams p;  // band 3, chance 30, steps A D I S

    BOOST_TEST(AlarmEligible(p, 0));   // A
    BOOST_TEST(AlarmEligible(p, 3));   // D
    BOOST_TEST(AlarmEligible(p, 8));   // I
    BOOST_TEST(AlarmEligible(p, 18));  // S
    BOOST_TEST(!AlarmEligible(p, 5));  // F

    BOOST_TEST(AlarmTriggered(p, 18, 70, 70, 30));
    BOOST_TEST(AlarmTriggered(p, 18, 73, 70, 1));   // band edge, failure side
    BOOST_TEST(AlarmTriggered(p, 18, 67, 70, 1));   // band edge, success side
    BOOST_TEST(!AlarmTriggered(p, 18, 74, 70, 1));  // outside band
    BOOST_TEST(!AlarmTriggered(p, 18, 66, 70, 1));
    BOOST_TEST(!AlarmTriggered(p, 18, 70, 70, 31)); // chance roll missed
    BOOST_TEST(!AlarmTriggered(p, 5, 70, 70, 1));   // step not alarm-capable
}

BOOST_AUTO_TEST_CASE(ai_policy)
{
    DecisionParams p;

    // Life threat: ground team if affordable and unused, else abort.
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::LifeThreat, 50, false) == CrisisChoice::GroundTeam));
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::LifeThreat, 10, false) == CrisisChoice::GroundTeam));
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::LifeThreat, 9, false) == CrisisChoice::Abort));
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::LifeThreat, 50, true) == CrisisChoice::Abort));
    // Other classes: no intervention.
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::MissionThreat, 50, false) == CrisisChoice::Accept));
    BOOST_TEST((AiCrisisChoice(p, CrisisClass::Minor, 50, false) == CrisisChoice::Accept));

    BOOST_TEST((AiAlarmChoice(p, 75) == AlarmOption::Go));
    BOOST_TEST((AiAlarmChoice(p, 99) == AlarmOption::Go));
    BOOST_TEST((AiAlarmChoice(p, 74) == AlarmOption::Abort));
}

BOOST_AUTO_TEST_CASE(fname_matching)
{
    BOOST_TEST(FNameMatches("F008", "F008"));
    BOOST_TEST(FNameMatches("F?08", "F108"));
    BOOST_TEST(FNameMatches("f?08", "F008"));
    BOOST_TEST(!FNameMatches("F?08", "F009"));
    BOOST_TEST(!FNameMatches("F?08", "F08"));
}

BOOST_AUTO_TEST_CASE(parse_defaults_and_overrides)
{
    DecisionData d;
    std::string err;
    BOOST_TEST(ParseDecisionData("{}", d, &err));
    BOOST_TEST(d.params.alarmBand == 3);
    BOOST_TEST(!d.genericLife.title.empty());

    BOOST_TEST(ParseDecisionData(
                   R"({"params": {"alarm_band": 5, "ground_cost_life": 7,
                         "alarm_steps": "S", "escalate_after_continue": false},
                       "abort_base": {"default": 11, "by_step": {"S": 99}},
                       "generic": {"life": {"title": "T", "text": "X"}}})",
                   d, &err));
    BOOST_TEST(d.params.alarmBand == 5);
    BOOST_TEST(d.params.groundCostLife == 7);
    BOOST_TEST(d.params.alarmSteps == "S");
    BOOST_TEST(!d.params.escalateAfterContinue);
    BOOST_TEST(d.abortBaseDefault == 11);
    BOOST_TEST(AbortBase(d, 18) == 99);
    BOOST_TEST(AbortBase(d, 0) == 25);  // default table kept
    BOOST_TEST(d.genericLife.title == "T");
    BOOST_TEST(d.params.groundCostMission == 5);  // untouched default

    BOOST_TEST(!ParseDecisionData("{ not json", d, &err));
    BOOST_TEST(!err.empty());
    BOOST_TEST(!ParseDecisionData("[]", d, &err));
}

BOOST_AUTO_TEST_CASE(shipped_decisions_json_loads)
{
    ResetDecisionData();
    const DecisionData& d = GetDecisionData();
    DecisionParams defaults;

    // The shipped file keeps the defaults in code.
    BOOST_TEST(d.params.alarmBand == defaults.alarmBand);
    BOOST_TEST(d.params.alarmChance == defaults.alarmChance);
    BOOST_TEST(d.params.alarmSteps == defaults.alarmSteps);
    BOOST_TEST(d.params.groundCostLife == defaults.groundCostLife);
    BOOST_TEST(d.params.continueMalus == defaults.continueMalus);
    BOOST_TEST(d.scenarios.size() >= 4);

    for (const auto& s : d.scenarios) {
        BOOST_TEST(!s.id.empty());
        BOOST_TEST(!s.fnames.empty(), s.id);
        BOOST_TEST(!s.text.title.empty(), s.id);
        BOOST_TEST(s.text.text.size() < 300, s.id);
    }
}

BOOST_AUTO_TEST_CASE(scenario_lookup)
{
    ResetDecisionData();
    const DecisionData& d = GetDecisionData();

    // MA-6 on reentry (alarm)
    ScenarioText t = FindScenario(d, "alarm", "F003", -1, CrisisClass::Minor);
    BOOST_TEST(t.title.find("HEAT SHIELD") != std::string::npos);

    // Apollo 11 on lunar landing (alarm), both F018 and F118
    t = FindScenario(d, "alarm", "F018", -1, CrisisClass::Minor);
    BOOST_TEST(t.title.find("1201") != std::string::npos);
    t = FindScenario(d, "alarm", "F118", -1, CrisisClass::Minor);
    BOOST_TEST(t.title.find("1201") != std::string::npos);

    // Gemini 8 on docking and Earth orbit activities
    t = FindScenario(d, "crisis", "F008", 33, CrisisClass::LifeThreat);
    BOOST_TEST(t.title.find("THRUSTER") != std::string::npos);
    t = FindScenario(d, "crisis", "F005", 3, CrisisClass::LifeThreat);
    BOOST_TEST(t.title.find("THRUSTER") != std::string::npos);
    // ... but a mission-class problem on F005 gets the generic text
    t = FindScenario(d, "crisis", "F005", 4, CrisisClass::MissionThreat);
    BOOST_TEST(t.title == d.genericMission.title);

    // Apollo 13 on TLI and lunar mid-course, life threat only
    t = FindScenario(d, "crisis", "F009", 3, CrisisClass::LifeThreat);
    BOOST_TEST(t.title.find("SERVICE MODULE") != std::string::npos);
    t = FindScenario(d, "crisis", "F014", 31, CrisisClass::LifeThreat);
    BOOST_TEST(t.title.find("SERVICE MODULE") != std::string::npos);
    t = FindScenario(d, "crisis", "F009", 15, CrisisClass::MissionThreat);
    BOOST_TEST(t.title == d.genericMission.title);

    // Generic fallbacks
    t = FindScenario(d, "crisis", "F012", 3, CrisisClass::LifeThreat);
    BOOST_TEST(t.title == d.genericLife.title);
    t = FindScenario(d, "alarm", "F012", -1, CrisisClass::Minor);
    BOOST_TEST(t.title == d.genericAlarm.title);
}

BOOST_AUTO_TEST_CASE(code_specific_scenario_beats_general)
{
    DecisionData d = DefaultDecisionData();
    DecisionScenario general;
    general.kind = "crisis";
    general.fnames = {"F?08"};
    general.text = {"GENERAL", "G"};
    DecisionScenario specific = general;
    specific.codes = {33};
    specific.text = {"SPECIFIC", "S"};
    d.scenarios = {general, specific};

    BOOST_TEST(FindScenario(d, "crisis", "F008", 33, CrisisClass::LifeThreat).title == "SPECIFIC");
    BOOST_TEST(FindScenario(d, "crisis", "F008", 26, CrisisClass::MissionThreat).title == "GENERAL");
}

BOOST_AUTO_TEST_CASE(mission_state_reset)
{
    CrisisMission().groundUsed = true;
    CrisisMission().malus = 10;
    CrisisMission().escalate = true;
    CrisisBeginMission();
    BOOST_TEST(!CrisisMission().groundUsed);
    BOOST_TEST(CrisisMission().malus == 0);
    BOOST_TEST(!CrisisMission().escalate);
}

BOOST_AUTO_TEST_CASE(prestige_delta_from_stored_steps)
{
    ResetDecisionData();
    memset(Mev, 0, sizeof Mev);

    // trace: 0 -> 1 -> 2 -> end
    Mev[0].trace = 1;
    Mev[1].trace = 2;
    Mev[2].trace = 0x7F;
    BOOST_TEST(CrisisPrestigeDelta() == 0);

    Mev[1].StepInfo = STEPINFO_ABORT_LIFE;
    BOOST_TEST(CrisisPrestigeDelta() == 2);

    Mev[1].StepInfo = STEPINFO_ABORT;  // mission-class abort: no bonus
    BOOST_TEST(CrisisPrestigeDelta() == 0);

    Mev[1].StepInfo = 1;
    Mev[1].rnum = CRISIS_RESCUED_LIFE_MARK;  // ground team rescue
    BOOST_TEST(CrisisPrestigeDelta() == 2);

    Mev[1].rnum = 0;
    Mev[2].StepInfo = STEPINFO_ALARM_ABORT;
    BOOST_TEST(CrisisPrestigeDelta() == -1);

    // steps off the traced path do not count
    Mev[2].StepInfo = 1;
    Mev[5].StepInfo = STEPINFO_ABORT_LIFE;
    BOOST_TEST(CrisisPrestigeDelta() == 0);

    memset(Mev, 0, sizeof Mev);
}

BOOST_AUTO_TEST_CASE(stepinfo_codes_are_partial_failures)
{
    // place.cpp: 500 <= spResult < 1999 reads PARTIAL FAILURE; deaths start
    // at 3000, so none of the new codes may collide with those ranges or
    // with codes used by FailEval().
    for (int c : {STEPINFO_ABORT, STEPINFO_ABORT_LIFE, STEPINFO_ALARM_ABORT,
                  STEPINFO_CONTINUED}) {
        BOOST_TEST(c >= 500);
        BOOST_TEST(c < 1999);
    }

    BOOST_TEST(CRISIS_RESCUED_LIFE_MARK > 10000);
}

BOOST_AUTO_TEST_SUITE_END()
