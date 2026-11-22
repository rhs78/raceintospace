/*
    Crisis system: player decisions during manned missions.
    See mis_d.h and doc/fork/krisensystem.md.
*/

#include "mis_d.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>

#include <json/json.h>

#include "display/graphics.h"
#include "display/surface.h"

#include "Buzz_inc.h"
#include "draw.h"
#include "fs.h"
#include "game_main.h"
#include "gr.h"
#include "mc.h"
#include "options.h"
#include "pace.h"
#include "sdlhelper.h"
#include "utils.h"

LOG_DEFAULT_CATEGORY(mission)

// from mis_m.cpp
#define F_INJ 1
void F_IRCrew(char mode, Astros* Guy);

// from mis_c.cpp
void InRFBox(int a, int b, int c, int d, int col);

// ---------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------

DecisionData DefaultDecisionData()
{
    DecisionData d;
    d.abortBase = {
        {'A', 25}, {'B', 50}, {'C', 60}, {'D', 10}, {'E', 15}, {'F', 70},
        {'G', 60}, {'H', 70}, {'I', 65}, {'J', 40}, {'K', 20}, {'L', 35},
        {'M', 65}, {'N', 45}, {'O', 45}, {'P', 70}, {'Q', 55}, {'R', 45},
        {'S', 40}, {'T', 15}, {'U', 70}
    };
    d.abortSystems = {
        {"escape_tower", {70, -1, 0, true, "ESCAPE TOWER"}},
        {"ejection_seats", {50, -1, 25, true, "EJECTION SEATS"}},
        {"none", {-1, -1, 0, false, "NO ESCAPE SYSTEM"}},
        {"ascent_stage", {-1, 55, 0, true, "ASCENT STAGE"}}
    };
    d.genericLife = {
        "EMERGENCY: CREW IN DANGER",
        "A SERIOUS FAULT THREATENS THE CREW. FLIGHT CONTROL MUST DECIDE "
        "HOW TO RESPOND."
    };
    d.genericMission = {
        "PROBLEM: MISSION AT RISK",
        "A FAULT ENDANGERS THE MISSION OBJECTIVE. FLIGHT CONTROL MUST "
        "DECIDE HOW TO RESPOND."
    };
    d.genericAlarm = {
        "WARNING: GO / NO-GO",
        "A WARNING LIGHT SHOWS ON THE CONSOLE. THE READINGS ARE "
        "BORDERLINE. FLIGHT CONTROL MUST DECIDE: GO OR ABORT."
    };
    return d;
}


namespace
{

void ReadInt(const Json::Value& v, const char* key, int& target)
{
    if (v.isObject() && v[key].isNumeric()) {
        target = v[key].asInt();
    }
}

void ReadBool(const Json::Value& v, const char* key, bool& target)
{
    if (v.isObject() && v[key].isBool()) {
        target = v[key].asBool();
    }
}

void ReadString(const Json::Value& v, const char* key, std::string& target)
{
    if (v.isObject() && v[key].isString()) {
        target = v[key].asString();
    }
}

void ReadText(const Json::Value& v, ScenarioText& target)
{
    ReadString(v, "title", target.title);
    ReadString(v, "text", target.text);
}

std::vector<std::string> ReadStringList(const Json::Value& v, const char* key)
{
    std::vector<std::string> out;

    if (v.isObject() && v[key].isArray()) {
        for (const auto& item : v[key]) {
            if (item.isString()) {
                out.push_back(item.asString());
            }
        }
    }

    return out;
}

DecisionData* g_data = nullptr;

} // namespace


bool ParseDecisionData(const std::string& json, DecisionData& out,
                       std::string* error)
{
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value doc;
    std::string errs;

    if (!reader->parse(json.data(), json.data() + json.size(), &doc, &errs)) {
        if (error) {
            *error = errs;
        }

        return false;
    }

    if (!doc.isObject()) {
        if (error) {
            *error = "decisions.json: top-level value must be an object";
        }

        return false;
    }

    DecisionData d = DefaultDecisionData();
    DecisionParams& p = d.params;
    const Json::Value& jp = doc["params"];

    ReadInt(jp, "abort_safety_divisor", p.abortSafetyDivisor);
    ReadInt(jp, "abort_min", p.abortMin);
    ReadInt(jp, "abort_max", p.abortMax);
    ReadInt(jp, "ground_cost_life", p.groundCostLife);
    ReadInt(jp, "ground_cost_mission", p.groundCostMission);
    ReadInt(jp, "ground_progress_divisor", p.groundProgressDivisor);
    ReadInt(jp, "ground_penalty_life", p.groundPenaltyLife);
    ReadInt(jp, "ground_penalty_mission", p.groundPenaltyMission);
    ReadInt(jp, "ground_min", p.groundMin);
    ReadInt(jp, "ground_max", p.groundMax);
    ReadInt(jp, "continue_malus", p.continueMalus);
    ReadBool(jp, "escalate_after_continue", p.escalateAfterContinue);
    ReadInt(jp, "hint_reliability", p.hintReliability);
    ReadInt(jp, "hint_min", p.hintMin);
    ReadInt(jp, "hint_max", p.hintMax);
    ReadInt(jp, "alarm_band", p.alarmBand);
    ReadInt(jp, "alarm_chance", p.alarmChance);
    ReadString(jp, "alarm_steps", p.alarmSteps);
    ReadInt(jp, "ai_alarm_go_safety", p.aiAlarmGoSafety);
    ReadInt(jp, "prestige_saved_crew", p.prestigeSavedCrew);
    ReadInt(jp, "prestige_alarm_abort", p.prestigeAlarmAbort);
    ReadInt(jp, "outlook_good", p.outlookGood);
    ReadInt(jp, "outlook_fair", p.outlookFair);

    // Guard against nonsensical divisors.
    p.abortSafetyDivisor = std::max(1, p.abortSafetyDivisor);
    p.groundProgressDivisor = std::max(1, p.groundProgressDivisor);

    const Json::Value& ja = doc["abort_base"];

    if (ja.isObject()) {
        ReadInt(ja, "default", d.abortBaseDefault);
        const Json::Value& steps = ja["by_step"];

        if (steps.isObject()) {
            for (const auto& name : steps.getMemberNames()) {
                if (name.size() == 1 && steps[name].isNumeric()) {
                    d.abortBase[name[0]] = steps[name].asInt();
                }
            }
        }
    }

    const Json::Value& jsys = doc["abort_systems"];

    if (jsys.isObject()) {
        for (const auto& name : jsys.getMemberNames()) {
            const Json::Value& s = jsys[name];

            if (!s.isObject()) {
                continue;
            }

            AbortSystemParams& a = d.abortSystems[name];
            ReadInt(s, "launch_base", a.launchBase);
            ReadInt(s, "landing_base", a.landingBase);
            ReadInt(s, "injury_chance", a.injuryChance);
            ReadBool(s, "available_at_launch", a.availableAtLaunch);
            ReadString(s, "label", a.label);
            a.injuryChance = std::max(0, std::min(100, a.injuryChance));
        }
    }

    const Json::Value& jg = doc["generic"];

    if (jg.isObject()) {
        ReadText(jg["life"], d.genericLife);
        ReadText(jg["mission"], d.genericMission);
        ReadText(jg["alarm"], d.genericAlarm);
    }

    const Json::Value& js = doc["scenarios"];

    if (js.isArray()) {
        for (const auto& item : js) {
            if (!item.isObject()) {
                continue;
            }

            DecisionScenario s;
            s.kind = "crisis";
            ReadString(item, "id", s.id);
            ReadString(item, "kind", s.kind);
            s.fnames = ReadStringList(item, "fnames");
            s.classes = ReadStringList(item, "classes");

            if (item["codes"].isArray()) {
                for (const auto& c : item["codes"]) {
                    if (c.isNumeric()) {
                        s.codes.push_back(c.asInt());
                    }
                }
            }

            ReadText(item, s.text);

            if (s.text.title.empty() && s.text.text.empty()) {
                continue;
            }

            d.scenarios.push_back(s);
        }
    }

    out = d;
    return true;
}


const DecisionData& GetDecisionData()
{
    if (g_data) {
        return *g_data;
    }

    g_data = new DecisionData(DefaultDecisionData());
    std::string path = locate_file("decisions.json", FT_DATA);

    if (path.empty()) {
        LOG_WARNING("decisions.json not found, using built-in defaults");
        return *g_data;
    }

    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string error;

    if (!ParseDecisionData(buffer.str(), *g_data, &error)) {
        LOG_ERROR("decisions.json: %s", error.c_str());
        *g_data = DefaultDecisionData();
    }

    return *g_data;
}


void ResetDecisionData()
{
    delete g_data;
    g_data = nullptr;
}


// ---------------------------------------------------------------------
// Pure logic
// ---------------------------------------------------------------------

CrisisClass ClassifyFailure(int failCode)
{
    switch (failCode) {
    case 3: case 5: case 13: case 16:
    case 17: case 22: case 31: case 33:
        return CrisisClass::LifeThreat;

    case 2: case 4: case 15: case 26: case 30:
        return CrisisClass::MissionThreat;

    default:
        return CrisisClass::Minor;
    }
}


CrisisClass Escalate(CrisisClass c)
{
    switch (c) {
    case CrisisClass::Minor:
        return CrisisClass::MissionThreat;

    case CrisisClass::MissionThreat:
    case CrisisClass::LifeThreat:
    default:
        return CrisisClass::LifeThreat;
    }
}


char StepLetter(int loc)
{
    // Inverse of the mapping in MissionCodes(): 'A'-'Z' -> 0-25,
    // 'a'... -> 26...
    if (loc < 0) {
        return '?';
    }

    if (loc <= 25) {
        return (char)('A' + loc);
    }

    return (char)('a' + (loc - 26));
}


static int Clamp(int v, int lo, int hi)
{
    return std::max(lo, std::min(v, hi));
}


int AbortBase(const DecisionData& d, int loc)
{
    auto it = d.abortBase.find(StepLetter(loc));
    return (it != d.abortBase.end()) ? it->second : d.abortBaseDefault;
}


int AbortChance(const DecisionData& d, int loc, int hardwareSafety,
                int crewBonus)
{
    const DecisionParams& p = d.params;
    int chance = AbortBase(d, loc)
                 + std::max(0, hardwareSafety) / p.abortSafetyDivisor
                 + crewBonus;
    return Clamp(chance, p.abortMin, p.abortMax);
}


const AbortSystemParams* AbortSystemFor(const DecisionData& d, int loc,
                                        const std::string& system)
{
    if (system.empty()) {
        return nullptr;
    }

    const bool launch = (loc == 0 && system != "ascent_stage");
    const bool landing = (loc == 18 && system == "ascent_stage");

    if (!launch && !landing) {
        return nullptr;
    }

    auto it = d.abortSystems.find(system);
    return (it != d.abortSystems.end()) ? &it->second : nullptr;
}


bool AbortAvailable(const DecisionData& d, int loc, const std::string& system)
{
    const AbortSystemParams* a = AbortSystemFor(d, loc, system);
    return a == nullptr || a->availableAtLaunch || loc != 0;
}


int AbortBase(const DecisionData& d, int loc, const std::string& system)
{
    const AbortSystemParams* a = AbortSystemFor(d, loc, system);

    if (a != nullptr) {
        const int base = (loc == 0) ? a->launchBase : a->landingBase;

        if (base >= 0) {
            return base;
        }
    }

    return AbortBase(d, loc);
}


int AbortChance(const DecisionData& d, int loc, const std::string& system,
                int hardwareSafety, int crewBonus)
{
    const DecisionParams& p = d.params;
    int chance = AbortBase(d, loc, system)
                 + std::max(0, hardwareSafety) / p.abortSafetyDivisor
                 + crewBonus;
    return Clamp(chance, p.abortMin, p.abortMax);
}


std::string AbortLabel(const DecisionData& d, int loc,
                       const std::string& system)
{
    const AbortSystemParams* a = AbortSystemFor(d, loc, system);

    if (a == nullptr || a->label.empty()) {
        return "ABORT MISSION";
    }

    return "ABORT: " + a->label;
}


std::vector<bool> RollAbortInjuries(int crew, int injuryChance,
                                    const std::function<int()>& roll100)
{
    std::vector<bool> injured(std::max(0, crew), false);

    if (injuryChance <= 0) {
        return injured;
    }

    for (size_t i = 0; i < injured.size(); ++i) {
        injured[i] = (roll100() <= injuryChance);
    }

    return injured;
}


int GroundCost(const DecisionParams& p, CrisisClass c)
{
    return (c == CrisisClass::LifeThreat) ? p.groundCostLife
                                          : p.groundCostMission;
}


int GroundBonus(const DecisionParams& p, int crewBonus, int hardwareSafety,
                int hardwareBase)
{
    int progress = std::max(0, hardwareSafety - hardwareBase);
    return crewBonus + progress / p.groundProgressDivisor;
}


int GroundFixChance(const DecisionParams& p, CrisisClass c, int stepSafety,
                    int bonus)
{
    int penalty = (c == CrisisClass::LifeThreat) ? p.groundPenaltyLife
                                                 : p.groundPenaltyMission;
    return Clamp(stepSafety + bonus - penalty, p.groundMin, p.groundMax);
}


int HintReliability(const DecisionParams& p, int crewBonus)
{
    return Clamp(p.hintReliability + crewBonus, p.hintMin, p.hintMax);
}


Outlook OutlookOf(const DecisionParams& p, int percent)
{
    if (percent >= p.outlookGood) {
        return Outlook::Good;
    }

    if (percent >= p.outlookFair) {
        return Outlook::Fair;
    }

    return Outlook::Poor;
}


const char* OutlookName(Outlook o)
{
    switch (o) {
    case Outlook::Good: return "GOOD";
    case Outlook::Fair: return "FAIR";
    default:            return "POOR";
    }
}


bool AlarmEligible(const DecisionParams& p, int loc)
{
    return p.alarmSteps.find(StepLetter(loc)) != std::string::npos;
}


bool AlarmTriggered(const DecisionParams& p, int loc, int dice, int safety,
                    int roll)
{
    if (!AlarmEligible(p, loc)) {
        return false;
    }

    if (std::abs(dice - safety) > p.alarmBand) {
        return false;
    }

    return roll <= p.alarmChance;
}


AlarmOption AlarmHint(const DecisionParams& p, bool problem, int crewBonus,
                      int roll)
{
    AlarmOption truth = problem ? AlarmOption::Abort : AlarmOption::Go;
    bool reliable = roll <= HintReliability(p, crewBonus);

    if (reliable) {
        return truth;
    }

    return (truth == AlarmOption::Go) ? AlarmOption::Abort : AlarmOption::Go;
}


CrisisChoice AiCrisisChoice(const DecisionParams& p, CrisisClass c,
                            int cash, bool groundUsed, bool abortAvailable)
{
    if (c == CrisisClass::LifeThreat) {
        if (!groundUsed && cash >= GroundCost(p, c)) {
            return CrisisChoice::GroundTeam;
        }

        return abortAvailable ? CrisisChoice::Abort : CrisisChoice::Accept;
    }

    return CrisisChoice::Accept;
}


AlarmOption AiAlarmChoice(const DecisionParams& p, int safety,
                          bool abortAvailable)
{
    if (!abortAvailable) {
        return AlarmOption::Go;
    }

    return (safety >= p.aiAlarmGoSafety) ? AlarmOption::Go
                                         : AlarmOption::Abort;
}


bool FNameMatches(const std::string& pattern, const std::string& fname)
{
    if (pattern.size() != fname.size()) {
        return false;
    }

    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '?') {
            continue;
        }

        if (std::toupper((unsigned char)pattern[i])
            != std::toupper((unsigned char)fname[i])) {
            return false;
        }
    }

    return true;
}


static const char* ClassName(CrisisClass c)
{
    return (c == CrisisClass::LifeThreat) ? "life" : "mission";
}


ScenarioText FindScenario(const DecisionData& d, const std::string& kind,
                          const std::string& fname, int code,
                          CrisisClass cls)
{
    const DecisionScenario* best = nullptr;
    int bestScore = -1;

    for (const auto& s : d.scenarios) {
        if (s.kind != kind) {
            continue;
        }

        bool nameOk = false;

        for (const auto& pat : s.fnames) {
            if (FNameMatches(pat, fname)) {
                nameOk = true;
                break;
            }
        }

        if (!nameOk) {
            continue;
        }

        int score = 1;

        if (!s.codes.empty()) {
            if (std::find(s.codes.begin(), s.codes.end(), code) == s.codes.end()) {
                continue;
            }

            score += 2;
        }

        if (!s.classes.empty()) {
            if (std::find(s.classes.begin(), s.classes.end(), ClassName(cls))
                == s.classes.end()) {
                continue;
            }

            score += 1;
        }

        if (score > bestScore) {  // earlier entries win ties
            best = &s;
            bestScore = score;
        }
    }

    if (best) {
        return best->text;
    }

    if (kind == "alarm") {
        return d.genericAlarm;
    }

    return (cls == CrisisClass::LifeThreat) ? d.genericLife
                                            : d.genericMission;
}


// ---------------------------------------------------------------------
// Per-mission state
// ---------------------------------------------------------------------

static CrisisState g_state;

CrisisState& CrisisMission()
{
    return g_state;
}


void CrisisBeginMission()
{
    g_state = CrisisState();
}


CrisisClass ApplyEscalation(CrisisState& s, CrisisClass c)
{
    if (!s.escalate) {
        return c;
    }

    s.escalate = false;
    return Escalate(c);
}


int CrisisPrestigeDelta()
{
    const DecisionParams& p = GetDecisionData().params;
    int delta = 0;
    int guard = 0;

    for (int i = 0; i != 0x7F && i >= 0 && i < 60 && guard < 64;
         i = Mev[i].trace, ++guard) {
        if (Mev[i].StepInfo == STEPINFO_ABORT_LIFE
            || Mev[i].rnum == CRISIS_RESCUED_LIFE_MARK) {
            delta += p.prestigeSavedCrew;
        } else if (Mev[i].StepInfo == STEPINFO_ALARM_ABORT) {
            delta += p.prestigeAlarmAbort;
        }
    }

    return delta;
}


// ---------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------

namespace
{

struct UiOption {
    std::string label;
    char key;           /**< shortcut, upper case */
    std::string info;   /**< right-aligned detail (cost, outlook) */
    bool enabled;
};

/** Draw word-wrapped text, at most maxLines lines. */
void DrawWrapped(int left, int top, const std::string& text, int maxWidth,
                 int lineHeight, int maxLines)
{
    std::istringstream words(text);
    std::string word, line;
    int ypos = top;
    int lines = 0;

    auto flush = [&]() {
        if (!line.empty() && lines < maxLines) {
            draw_string(left, ypos, line.c_str());
            ypos += lineHeight;
            ++lines;
        }

        line.clear();
    };

    while (words >> word) {
        std::string trial = line.empty() ? word : line + " " + word;

        if (!line.empty() && TextDisplayLength(trial.c_str()) > maxWidth) {
            flush();
            line = word;
        } else {
            line = trial;
        }
    }

    flush();
}

const int kOptTop = 112;
const int kOptStep = 17;

bool InRect(int x1, int y1, int x2, int y2)
{
    return ::x >= x1 && ::x <= x2 && ::y >= y1 && ::y <= y2;
}

void WaitContinue()
{
    WaitForMouseUp();
    key = 0;

    while (1) {
        GetMouse();

        if ((InRect(245, 5, 314, 17) && mousebuttons > 0) || key == K_ENTER
            || key == K_SPACE || key == K_ESCAPE) {
            InBox(245, 5, 314, 17);
            WaitForMouseUp();
            OutBox(245, 5, 314, 17);
            delay(10);
            key = 0;
            return;
        }
    }
}

/**
 * Show a dialog and let the player pick an option.
 *
 * \param resolve  called with the chosen index; returns the result text
 *                 to show on a second screen (empty = no second screen).
 * \return the index chosen.
 */
int RunDialog(char plr, const char* heading, const MisEval& step,
              const ScenarioText& scenario, const std::vector<UiOption>& opts,
              const std::string& footer, int footerColor,
              const std::function<std::string(int)>& resolve)
{
    display::LegacySurface saveScreen(display::graphics.screen()->width(),
                                      display::graphics.screen()->height());
    FadeOut(2, 10, 0, 0);
    saveScreen.copyFrom(display::graphics.legacyScreen(), 0, 0,
                        display::graphics.screen()->width() - 1,
                        display::graphics.screen()->height() - 1);

    display::graphics.screen()->clear();
    ShBox(0, 0, 319, 22);
    InBox(3, 3, 30, 19);
    draw_small_flag(plr, 4, 4);
    draw_heading(45, 5, heading, 0, -1);
    ShBox(0, 24, 319, 199);

    // Mission and step
    InRFBox(4, 27, 315, 47, 0);
    display::graphics.setForegroundColor(1);
    draw_string(9, 35, Data->P[plr].Mission[step.pad].Name);
    display::graphics.setForegroundColor(11);
    draw_string(9, 43, "STEP: ");
    draw_string(0, 0, S_Name[step.loc]);

    // Scenario
    InRFBox(4, 50, 315, 108, 0);
    display::graphics.setForegroundColor(9);
    draw_string(9, 59, scenario.title.c_str());
    display::graphics.setForegroundColor(11);
    DrawWrapped(9, 68, scenario.text, 296, 7, 6);

    // Options
    for (size_t i = 0; i < opts.size(); ++i) {
        int top = kOptTop + (int)i * kOptStep;
        IOBox(8, top, 311, top + 14);
        display::graphics.setForegroundColor(opts[i].enabled ? 1 : 8);
        draw_string_keyhint(14, top + 9, opts[i].label.c_str(), opts[i].key,
                            opts[i].enabled ? 11 : 8);
        display::graphics.setForegroundColor(opts[i].enabled ? 11 : 8);
        draw_string(306, top + 9, opts[i].info.c_str(), ALIGN_RIGHT);
    }

    // Footer (flight director hint etc.)
    if (!footer.empty()) {
        InRFBox(4, 184, 315, 196, 0);
        display::graphics.setForegroundColor(footerColor);
        draw_string(160, 192, footer.c_str(), ALIGN_CENTER);
    }

    FadeIn(2, 10, 0, 0);
    WaitForMouseUp();
    key = 0;

    while (bioskey(1)) {
        bioskey(0);
    }

    int chosen = -1;

    while (chosen < 0) {
        GetMouse();

        for (size_t i = 0; i < opts.size(); ++i) {
            if (!opts[i].enabled) {
                continue;
            }

            int top = kOptTop + (int)i * kOptStep;
            bool keyHit = (key > 0 && key < 256
                           && std::toupper(key) == opts[i].key);

            if (keyHit || (InRect(8, top, 311, top + 14) && mousebuttons > 0)) {
                InBox(8, top, 311, top + 14);
                WaitForMouseUp();
                OutBox(8, top, 311, top + 14);
                delay(10);
                chosen = (int)i;
                break;
            }
        }

        key = 0;
    }

    std::string result = resolve(chosen);

    if (!result.empty()) {
        InRFBox(4, 112, 315, 196, 0);
        ShBox(0, 0, 319, 22);
        InBox(3, 3, 30, 19);
        draw_small_flag(plr, 4, 4);
        IOBox(243, 3, 316, 19);
        display::graphics.setForegroundColor(1);
        draw_string(258, 13, "CONTINUE");
        draw_heading(45, 5, heading, 0, -1);
        display::graphics.setForegroundColor(11);
        DrawWrapped(12, 124, result, 296, 8, 8);
        WaitContinue();
    }

    FadeOut(2, 10, 0, 0);
    display::graphics.legacyScreen()->palette().copy_from(saveScreen.palette());
    display::graphics.screen()->draw(saveScreen, 0, 0);
    FadeIn(2, 10, 0, 0);
    key = 0;
    return chosen;
}

} // namespace


// ---------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------

namespace
{

int Roll100()
{
    return brandom(100) + 1;
}

}


CrisisOutcome OfferCrisis(const CrisisRequest& req)
{
    const DecisionData& data = GetDecisionData();
    const DecisionParams& p = data.params;
    const MisEval& step = *req.step;
    CrisisState& state = CrisisMission();
    auto& pData = Data->P[(int)req.plr];

    const int crewBonus = step.asf;
    const bool abortAvailable = AbortAvailable(data, step.loc,
                                               req.abortSystem);
    const int abortChance = AbortChance(data, step.loc, req.abortSystem,
                                        req.hardwareSafety, crewBonus);
    const std::string abortLabel = AbortLabel(data, step.loc,
                                              req.abortSystem);
    const AbortSystemParams* abortSys = AbortSystemFor(data, step.loc,
                                                       req.abortSystem);
    const int groundCost = GroundCost(p, req.cls);
    const int groundBonus = GroundBonus(p, crewBonus, req.hardwareCurrent,
                                        req.hardwareBase);
    const int groundChance = GroundFixChance(p, req.cls, req.safety,
                                             groundBonus);
    const bool canGround = !state.groundUsed && pData.Cash >= groundCost;
    const bool canContinue = (req.cls == CrisisClass::MissionThreat);

    CrisisOutcome outcome = CrisisOutcome::Accepted;

    // Rolls the dice and returns the result text for the second screen.
    auto execute = [&](CrisisChoice c) -> std::string {
        switch (c) {
        case CrisisChoice::Abort:
            if (!abortAvailable) {
                outcome = CrisisOutcome::Accepted;
                return "";
            }

            if (Roll100() <= abortChance) {
                outcome = CrisisOutcome::AbortOk;
                std::string text = "ABORT SUCCESSFUL";

                if (abortSys != nullptr && !abortSys->label.empty()) {
                    text += " WITH THE " + abortSys->label;
                }

                text += ". THE CREW RETURNS SAFELY, BUT THE MISSION "
                        "OBJECTIVES ARE NOT MET.";

                if (abortSys != nullptr && abortSys->injuryChance > 0) {
                    const int pad = step.pad;
                    std::vector<bool> hurt = RollAbortInjuries(
                                                 MANNED[pad],
                                                 abortSys->injuryChance,
                                                 Roll100);
                    int count = 0;

                    for (size_t k = 0; k < hurt.size() && k < 4; ++k) {
                        if (hurt[k] && MA[pad][k].A != nullptr) {
                            F_IRCrew(F_INJ, MA[pad][k].A);
                            ++count;
                        }
                    }

                    if (count > 0) {
                        text += " " + std::to_string(count)
                                + ((count == 1) ? " CREW MEMBER WAS"
                                   : " CREW MEMBERS WERE")
                                + " INJURED DURING THE EJECTION.";
                    }
                }

                return text;
            }

            outcome = CrisisOutcome::AbortFailed;

            if (abortSys != nullptr && !abortSys->label.empty()) {
                return "THE ABORT WITH THE " + abortSys->label
                       + " FAILED. THE ORIGINAL FAILURE TAKES ITS COURSE.";
            }

            return "THE ABORT FAILED. THE ORIGINAL FAILURE TAKES ITS COURSE.";

        case CrisisChoice::GroundTeam:
            pData.Cash -= groundCost;
            state.groundUsed = true;

            if (Roll100() <= groundChance) {
                outcome = CrisisOutcome::FixOk;
                return "THE GROUND TEAM FOUND A SOLUTION. THE MISSION "
                       "CONTINUES.";
            }

            outcome = CrisisOutcome::FixFailed;
            return "THE GROUND TEAM COULD NOT FIND A SOLUTION IN TIME. THE "
                   "ORIGINAL FAILURE TAKES ITS COURSE.";

        case CrisisChoice::Continue:
            outcome = CrisisOutcome::Continued;
            state.malus += p.continueMalus;
            state.escalate = p.escalateAfterContinue;
            return "THE MISSION CONTINUES AT RISK. SAFETY IS REDUCED FOR THE "
                   "REMAINING STEPS, AND THE NEXT PROBLEM WILL BE MORE "
                   "SERIOUS.";

        case CrisisChoice::Accept:
        default:
            outcome = CrisisOutcome::Accepted;
            return "";
        }
    };

    if (AI[(int)req.plr]) {
        execute(AiCrisisChoice(p, req.cls, pData.Cash, state.groundUsed,
                               abortAvailable));
        return outcome;
    }

    ScenarioText scenario = FindScenario(data, "crisis", step.FName,
                                         req.fail ? req.fail->code : -1,
                                         req.cls);

    // Show the concrete failure from fails.json first, then the
    // (historical or generic) crisis text.
    if (req.fail) {
        std::string failText(req.fail->text);

        if (!failText.empty()) {
            scenario.text = failText + " " + scenario.text;
        }
    }

    std::vector<UiOption> opts;
    std::vector<CrisisChoice> map;

    opts.push_back({abortLabel, 'A',
                    abortAvailable
                    ? std::string("CHANCE: ")
                    + OutlookName(OutlookOf(p, abortChance))
                    : std::string("NOT POSSIBLE"), abortAvailable});
    map.push_back(CrisisChoice::Abort);

    {
        std::ostringstream info;

        if (state.groundUsed) {
            info << "ALREADY USED";
        } else if (!canGround) {
            info << groundCost << " MB  NOT ENOUGH FUNDS";
        } else {
            info << groundCost << " MB  CHANCE: "
                 << OutlookName(OutlookOf(p, groundChance));
        }

        opts.push_back({"GROUND TEAM FIX", 'G', info.str(), canGround});
        map.push_back(CrisisChoice::GroundTeam);
    }

    if (canContinue) {
        std::ostringstream info;
        info << "-" << p.continueMalus << " SAFETY ON LATER STEPS";
        opts.push_back({"CONTINUE AT RISK", 'C', info.str(), true});
        map.push_back(CrisisChoice::Continue);
    }

    opts.push_back({"ACCEPT CONSEQUENCES", 'X', "AS THE DICE FELL", true});
    map.push_back(CrisisChoice::Accept);

    const char* heading = (req.cls == CrisisClass::LifeThreat)
                          ? "MISSION EMERGENCY" : "MISSION PROBLEM";

    RunDialog(req.plr, heading, step, scenario, opts, "", 11,
              [&](int idx) { return execute(map[idx]); });

    return outcome;
}


AlarmOption OfferAlarm(char plr, const MisEval& step, int dice, int safety,
                       bool problem, const std::string& abortSystem)
{
    const DecisionData& data = GetDecisionData();
    const DecisionParams& p = data.params;
    const bool abortAvailable = AbortAvailable(data, step.loc, abortSystem);

    if (AI[(int)plr]) {
        return AiAlarmChoice(p, safety, abortAvailable);
    }

    AlarmOption hint = AlarmHint(p, problem, step.asf, Roll100());
    ScenarioText scenario = FindScenario(data, "alarm", step.FName, -1,
                                         CrisisClass::Minor);

    std::vector<UiOption> opts;
    opts.push_back({"GO", 'G', "THE ROLL STANDS", true});
    opts.push_back({AbortLabel(data, step.loc, abortSystem) == "ABORT MISSION"
                    ? std::string("ABORT")
                    : AbortLabel(data, step.loc, abortSystem),
                    'A',
                    abortAvailable ? "SAFE END, MISSION FAILS"
                    : "NOT POSSIBLE", abortAvailable});

    std::string footer = (hint == AlarmOption::Go)
                         ? "FLIGHT DIRECTOR: GO RECOMMENDED"
                         : "FLIGHT DIRECTOR: ABORT RECOMMENDED";

    (void)dice;

    int idx = RunDialog(plr, "ALARM", step, scenario, opts, footer, 11,
    [](int i) {
        return (i == 1)
               ? std::string("MISSION CONTROL ORDERS A SAFE ABORT. THE CREW "
                             "IS SAFE, BUT THE MISSION IS LOST.")
               : std::string();
    });

    return (idx == 1) ? AlarmOption::Abort : AlarmOption::Go;
}
