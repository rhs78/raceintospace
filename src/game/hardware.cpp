/* Copyright (C) 2019
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "hardware.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include "game_main.h"
#include "hardware_catalog.h"
#include "macros.h"
#include "mission_util.h"
#include "options.h"
#include "scenario.h"


namespace
{

void CheckCategory(int equipmentIndex)
{
    if (equipmentIndex < PROBE_HARDWARE || equipmentIndex > MISC_HARDWARE) {
        throw std::invalid_argument(
            "Error in function HardwareCount: "
            "argument is not part of the EquipmentIndex enum");
    }
}

} // namespace


/**
 * The hardware list of a player for an equipment category.
 *
 * \throws std::invalid_argument  if equipmentIndex is not valid.
 */
std::vector<Equipment>& HardwareList(BuzzData& player, int equipmentIndex)
{
    CheckCategory(equipmentIndex);

    switch (equipmentIndex) {
    case PROBE_HARDWARE:
        return player.Probe;

    case ROCKET_HARDWARE:
        return player.Rocket;

    case MANNED_HARDWARE:
        return player.Manned;

    default:
        return player.Misc;
    }
}


const std::vector<Equipment>& HardwareList(const BuzzData& player,
                                           int equipmentIndex)
{
    return HardwareList(const_cast<BuzzData&>(player), equipmentIndex);
}


/**
 * Number of hardware programs of a player in an equipment category,
 * i.e. the size of the player's list.
 *
 * \throws std::invalid_argument  if equipmentIndex is not valid.
 */
int HardwareCount(int player, int equipmentIndex)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    return int(HardwareList(Data->P[player], equipmentIndex).size());
}


/**
 * Number of hardware programs in an equipment category: the longer of the
 * two players' lists in the game state, or, without a game state, the
 * size from the default (CLASSIC) scenario.
 *
 * \throws std::invalid_argument  if equipmentIndex is not valid.
 */
int HardwareCount(int equipmentIndex)
{
    CheckCategory(equipmentIndex);

    if (Data != nullptr) {
        size_t n = 0;

        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            n = std::max(n, HardwareList(Data->P[plr], equipmentIndex).size());
        }

        if (n > 0) {
            return int(n);
        }
    }

    return GetScenario(DefaultScenarioId()).count(equipmentIndex);
}


/**
 * Number of crewed programs of a player: capsules, minishuttles and
 * spaceplanes of the Manned list.  They are the first entries of the list
 * (ProgramToMannedIndex()).
 */
int ProgramCount(const BuzzData& player)
{
    int n = 0;

    for (const Equipment& e : player.Manned) {
        const HardwareEntry* entry = CatalogEntryForEquipment(e);

        if (entry != nullptr
            && (entry->role == HardwareRole::Capsule
                || entry->role == HardwareRole::Minishuttle
                || entry->role == HardwareRole::Spaceplane)) {
            n++;
        }
    }

    return n;
}


int ProgramCount(int player)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    return ProgramCount(Data->P[player]);
}


/**
 * Size the tables that hang on the hardware lists (Buy,
 * IntelHardwareTable, Crew, CrewCount) to the lists of the player.
 * Existing contents are kept; new entries are zero.
 */
void SizeHardwareTables(BuzzData& player)
{
    player.Buy.resize(4);
    player.IntelHardwareTable.resize(4);

    for (int c = 0; c < 4; c++) {
        const size_t n = HardwareList(player, c).size();
        player.Buy[c].resize(n, 0);
        player.IntelHardwareTable[c].resize(n, 0);
    }

    // One row per crewed program; row 0 is unused (program numbers start
    // at 1).
    const size_t rows = size_t(ProgramCount(player)) + 1;
    std::array<int8_t, MAX_CREWS_IN_PROGRAM> zeroCount;
    zeroCount.fill(0);
    std::array<std::array<int8_t, ASTRONAUT_FLT_CREW_MAX + 1>,
        MAX_CREWS_IN_PROGRAM> zeroCrew;

    for (auto& c : zeroCrew) {
        c.fill(0);
    }

    player.CrewCount.resize(rows, zeroCount);
    player.Crew.resize(rows, zeroCrew);
}


/**
 * The catalog entry of the program at a position of a player's list.
 * Without a game state (or with empty lists) the lists of the default
 * (CLASSIC) scenario define the positions.
 *
 * \return  nullptr if the position does not exist.
 */
const HardwareEntry* HardwareEntryAt(int player, int equipmentIndex, int unit)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    CheckCategory(equipmentIndex);

    if (Data == nullptr
        || HardwareList(Data->P[player], equipmentIndex).empty()) {
        const std::vector<int>& list =
            GetScenario(DefaultScenarioId()).list(player, equipmentIndex);

        if (unit < 0 || size_t(unit) >= list.size()) {
            return nullptr;
        }

        return GetHardwareCatalog().At(list[unit]);
    }

    const std::vector<Equipment>& list =
        HardwareList(Data->P[player], equipmentIndex);

    if (unit < 0 || size_t(unit) >= list.size()) {
        return nullptr;
    }

    return CatalogEntryForEquipment(list[unit]);
}


/**
 * Check that the hardware lists and tables of a game state are consistent
 * with the hardware catalog and the scenario of the state (used for loaded
 * save games).
 *
 * \throws std::out_of_range  if an entry is not in the catalog, is of the
 *   wrong side or category, or a table has the wrong size.
 * \throws std::runtime_error  if the scenario is unknown.
 */
void ValidateHardwareState(const Players& data)
{
    const HardwareCatalog& catalog = GetHardwareCatalog();
    ScenarioOf(data);  // throws for an unknown scenario

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        const BuzzData& p = data.P[plr];

        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            const std::vector<Equipment>& list = HardwareList(p, c);

            if (list.empty() || list.size() > 100) {
                throw std::out_of_range("hardware list has an invalid size");
            }

            for (const Equipment& e : list) {
                const HardwareEntry* entry = catalog.At(e.CatalogIndex);

                if (entry == nullptr || entry->player != plr
                    || entry->categoryIndex() != c) {
                    throw std::out_of_range(
                        "hardware program does not fit the catalog");
                }
            }

            if (p.Buy.size() != 4 || p.Buy[c].size() != list.size()
                || p.IntelHardwareTable.size() < 4
                || p.IntelHardwareTable[c].size() < list.size()) {
                throw std::out_of_range("hardware table has the wrong size");
            }
        }

        const size_t rows = size_t(ProgramCount(p)) + 1;

        if (p.Crew.size() != rows || p.CrewCount.size() != rows) {
            throw std::out_of_range("crew table has the wrong size");
        }
    }
}


/**
 * Whether the research and purchase screens offer a program: all but the
 * ones the catalog marks as not researchable (the photo reconnaissance
 * and, for the USA, the Kicker C that only the USSR has).
 */
bool IsResearchable(const Equipment& program)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr && e->researchable;
}


/**
 * The first program of a category the research and purchase screens offer.
 * 0 if the list has none.
 */
int FirstResearchableUnit(const BuzzData& player, int equipmentIndex)
{
    const std::vector<Equipment>& list = HardwareList(player, equipmentIndex);

    for (size_t i = 0; i < list.size(); i++) {
        if (IsResearchable(list[i])) {
            return int(i);
        }
    }

    return 0;
}


/**
 * The next (step > 0) or previous (step < 0) program of a category the
 * research and purchase screens offer, wrapping around the list.  The
 * list is the player's real list, so programs of a scenario beyond the
 * classic ones are reached too.
 *
 * \param unit  the current position in the list.
 * \return  the new position; the same one if no other program qualifies.
 */
int CycleResearchableUnit(const BuzzData& player, int equipmentIndex,
                          int unit, int step)
{
    const std::vector<Equipment>& list = HardwareList(player, equipmentIndex);
    const int n = int(list.size());

    if (n == 0) {
        return 0;
    }

    const int dir = step < 0 ? -1 : 1;
    int i = unit;

    for (int k = 0; k < n; k++) {
        i = ((i + dir) % n + n) % n;

        if (IsResearchable(list[i])) {
            return i;
        }
    }

    return unit;
}


/**
 * Position in the player's list (of the entry's category) of the program
 * of a catalog entry, -1 if the player does not have it.
 */
int HardwareListIndex(int player, const HardwareEntry& entry)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    const int catalogIndex = GetHardwareCatalog().IndexOf(entry.id);

    for (int c = 0; c < 4; c++) {
        const std::vector<Equipment>& list =
            HardwareList(Data->P[player], c);

        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].CatalogIndex == catalogIndex) {
                return int(i);
            }
        }
    }

    return -1;
}


/**
 * Positions in the player's hardware lists of all programs with a role,
 * in list order.  The programs may be in any category (e.g. probes in
 * Probe, docking module and kickers in Misc).
 */
std::vector<int> HardwareIndicesByRole(int player, HardwareRole role)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    std::vector<int> result;

    for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
        const std::vector<Equipment>& list =
            HardwareList(Data->P[player], c);

        for (size_t i = 0; i < list.size(); i++) {
            if (HasRole(list[i], role)) {
                result.push_back(int(i));
            }
        }

        if (!result.empty()) {
            break;  // a role lives in one category
        }
    }

    return result;
}


/** The first program with a role in the player's lists, -1 if none. */
int HardwareIndexByRole(int player, HardwareRole role)
{
    const std::vector<int> all = HardwareIndicesByRole(player, role);
    return all.empty() ? -1 : all.front();
}


/** Misc index of the docking module of the player (-1 if there is none). */
int DockingModuleIndex(int player)
{
    return HardwareIndexByRole(player, HardwareRole::DockingModule);
}


/** Rocket index of the boosters of the player (-1 if there is none). */
int BoosterIndex(int player)
{
    return HardwareIndexByRole(player, HardwareRole::Booster);
}


/** Misc index of the EVA suits of the player (-1 if there is none). */
int EvaSuitsIndex(int player)
{
    return HardwareIndexByRole(player, HardwareRole::Eva);
}


/**
 * Put the hardware of a mission into the "nothing assigned" state: every
 * slot -1, no docking module, no boosters.  A mission with a launch
 * vehicle (HasLaunchVehicle()) has its hardware assigned.
 */
void ClearMissionHardware(MissionType& mission)
{
    for (int8_t& h : mission.Hard) {
        h = -1;
    }

    mission.DockingModule = -1;
    mission.Booster = false;
}


/** Whether a rocket (and with it the payload) is assigned to a mission. */
bool HasLaunchVehicle(const MissionType& mission)
{
    return mission.Hard[Mission_Rocket] >= 0;
}


/**
 * The hardware program of one slot of a mission.
 *
 * \param slot  Mission_Capsule .. Mission_EVA; Mission_Probe_DM gives the
 *   probe, or the docking module if the mission has no probe.
 * \return  nullptr if the slot is empty.
 */
Equipment* MissionEquipment(int player, const MissionType& mission,
                            MissionHardwareType slot)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    BuzzData& p = Data->P[player];
    const int index = (slot <= Mission_EVA) ? mission.Hard[slot] : -1;
    auto at = [](std::vector<Equipment>& list, int i) -> Equipment* {
        return (i >= 0 && size_t(i) < list.size()) ? &list[i] : nullptr;
    };

    switch (slot) {
    case Mission_Capsule:
    case Mission_LM:
        return at(p.Manned, index);

    case Mission_Kicker:
    case Mission_EVA:
        return at(p.Misc, index);

    case Mission_Probe_DM:
        if (index >= 0) {
            return at(p.Probe, index);
        }

        return at(p.Misc, mission.DockingModule);

    case Mission_PrimaryBooster:
        return at(p.Rocket, index);

    case Mission_SecondaryBooster:
        return mission.Booster ? at(p.Rocket, BoosterIndex(player)) : nullptr;

    default:
        return nullptr;
    }
}


/**
 * Which of the three classic probe images (orbital 0, interplanetary 1,
 * lunar 2) shows a probe, per its role; -1 for no probe or a probe without
 * such a role.  The mission history images exist for these three only.
 */
int ProbeImageSlot(int player, int probeIndex)
{
    if (probeIndex < 0) {
        return -1;
    }

    const HardwareEntry* e = HardwareEntryAt(player, PROBE_HARDWARE, probeIndex);

    if (e == nullptr) {
        return -1;
    }

    switch (e->role) {
    case HardwareRole::OrbitalProbe:
        return 0;

    case HardwareRole::InterplanetaryProbe:
        return 1;

    case HardwareRole::LunarProbe:
        return 2;

    default:
        return -1;
    }
}


/**
 * Convert a program number (1 to 5, as used by the Manned program
 * hardware selection: Mercury/Vostok = 1 ... ) to an index into the
 * Manned array (0 to 4).
 */
int ProgramToMannedIndex(int program)
{
    return program - 1;
}


/** Inverse of ProgramToMannedIndex(). */
int MannedIndexToProgram(int mannedIndex)
{
    return mannedIndex + 1;
}


/**
 * Which capsule/minishuttle programs may fly a mission, ignoring whether
 * they have been purchased.
 *
 * The limits come from the hardware catalog (max_future_duration,
 * max_plan_days, max_min_equipment, reach, can_dock, direct_ascent,
 * soyuz_ll).  Before the catalog the two selection screens of crew.cpp
 * (HardRequest/SecondHard) used slightly different limits; they are
 * unified to the more generous (manned) rule.
 *
 * \param plr  0 for the USA, 1 for the USSR.
 * \param missionCode  the mission per MissionValues (mStr.Index).
 * \param futureDuration  Future[pad].Duration of the planned mission.
 * \return  true for each Manned[0..4] program that is suitable.
 */
std::array<bool, MANNED_PROGRAM_COUNT>
CapsuleEligibility(int plr, int missionCode, int futureDuration)
{
    std::array<bool, MANNED_PROGRAM_COUNT> ok;
    ok.fill(true);

    const mStr plan = GetMissionPlan(missionCode);

    // Capsules of this side that can fly the Soyuz lunar landing.
    bool sideHasSoyuzLL = false;

    for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
        const HardwareEntry* e = HardwareEntryAt(plr, MANNED_HARDWARE, i);

        if (e != nullptr && e->soyuzLL) {
            sideHasSoyuzLL = true;
        }
    }

    for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
        const HardwareEntry* e = HardwareEntryAt(plr, MANNED_HARDWARE, i);

        if (e == nullptr) {
            continue;
        }

        bool allowed = true;

        // Lunar missions need a capsule that reaches the Moon.
        if (plan.Lun == 1 && e->reach != "translunar"
            && e->reach != "interplanetary") {
            allowed = false;
        }

        // Docking missions need a capsule that can dock.
        if (plan.Doc == 1 && !e->canDock) {
            allowed = false;
        }

        if (e->maxMinEquipment >= 0 && plan.mEq > e->maxMinEquipment) {
            allowed = false;
        }

        if (e->maxPlanDays >= 0 && plan.Days > e->maxPlanDays) {
            allowed = false;
        }

        if (e->maxFutureDuration >= 0 && futureDuration > e->maxFutureDuration) {
            allowed = false;
        }

        // Only a direct ascent capsule can attempt a Direct Ascent
        // Lunar Landing.
        if (missionCode == Mission_DirectAscent_LL && !e->directAscent) {
            allowed = false;
        }

        // Only a Soyuz capsule may attempt a Soyuz Lunar Landing
        // (checked only for a side that has one: the USA has none).
        if (missionCode == Mission_Soyuz_LL && sideHasSoyuzLL && !e->soyuzLL) {
            allowed = false;
        }

        ok[i] = allowed;
    }

    return ok;
}


namespace
{

const HardwareEntry& CatalogEntry(const Equipment& program)
{
    const HardwareEntry* entry = CatalogEntryForEquipment(program);

    if (entry == nullptr) {
        throw std::invalid_argument(
            "Error in function HardwareType: "
            "program's hardware index is not recognized.");
    }

    return *entry;
}

} // namespace


/**
 * Check the crew capacity for a Capsule/Minishuttle or Lunar Module.
 *
 * \param program  a Manned hardware program.
 * \return  number of crew allowed/required (0 if unmanned).
 */
int CrewSize(const Equipment &program)
{
    // Throws std::invalid_argument for unrecognized hardware, like
    // HardwareType().
    const HardwareEntry& entry = CatalogEntry(program);
    return entry.crew;
}


/**
 * The mission slot (capsule, kicker, ...) a hardware program fills.
 *
 * \param program  a hardware program from the catalog.
 * \throws std::invalid_argument  if the program is not in the catalog.
 */
MissionHardwareType HardwareType(const Equipment &program)
{
    return CatalogEntry(program).missionType();
}


/**
 * Accessor for the player hardware programs.
 *
 * \param player  0 for the USA, 1 for the USSR
 * \param type    which category of hardware program, per EquipmentIndex
 * \param program the index of the hardware program, per the relevant
 *        Equip<type>Index enum
 */
Equipment &HardwareProgram(int player, int type, int program)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    assert(type >= 0 && type < 4);
    std::vector<Equipment>& list = HardwareList(Data->P[player], type);
    assert(program >= 0 && size_t(program) < list.size());
    return list.at(program);
}


/**
 * Calculates the safety rating for a rocket with boosters attachment.
 *
 * \param safetyRocket  The rocket program's Safety factor.
 * \param safetyBooster  The booster program's Safety factor.
 * \return  the Safety test value for the rocket/booster combination.
 */
int RocketBoosterSafety(int safetyRocket, int safetyBooster)
{
    if (options.boosterSafety == 0) {
        return ((safetyRocket * safetyBooster) / 100);
    }

    if (options.boosterSafety == 1) {
        return (MIN(safetyRocket, safetyBooster));
    } else {
        return ((safetyRocket + safetyBooster) / 2);
    }
}
int RocketMaxRD(int MaxRDRocket, int MaxRDBooster)
{
    if (options.boosterSafety == 0) {
        return ((MaxRDRocket * MaxRDBooster) / 100);
    }

    if (options.boosterSafety == 1) {
        return (MIN(MaxRDRocket, MaxRDBooster));
    } else {
        return ((MaxRDRocket + MaxRDBooster) / 2);
    }
}


namespace
{

/** Safety of the program with the given catalog id, for a player. */
int SafetyOf(int plr, const std::string& id)
{
    const int index = GetHardwareCatalog().IndexOf(id);

    if (index < 0) {
        throw std::invalid_argument("tech transfer: unknown program " + id);
    }

    for (int c = 0; c < 4; c++) {
        for (const Equipment& e : HardwareList(Data->P[plr], c)) {
            if (e.CatalogIndex == index) {
                return e.Safety;
            }
        }
    }

    // The scenario does not have this program: it contributes nothing.
    return 0;
}

/** All groups of a rule hold: some program of each group is at_least. */
bool RuleHolds(int plr, const TechTransferRule& rule)
{
    for (const auto& group : rule.when) {
        bool any = false;

        for (const std::string& id : group) {
            if (SafetyOf(plr, id) >= rule.atLeast) {
                any = true;
                break;
            }
        }

        if (!any) {
            return false;
        }
    }

    return true;
}

/** Safety of a new program computed by the gradual formula. */
int GradualTransfer(int plr, const TechTransfer& tt)
{
    float acc = 0.0f;

    for (const TechTransferStep& st : tt.steps) {
        switch (st.op) {
        case TechTransferStep::Op::Cap:
            if (acc > st.capMax) {
                acc = st.capTo;
            }

            break;

        case TechTransferStep::Op::Set: {
            const int n = SafetyOf(plr, st.from);

            if (n > st.above
                && (st.greaterThan.empty() || n > SafetyOf(plr, st.greaterThan))) {
                acc = (float)(n - st.offset) / (float)st.divisor;
            }

            break;
        }

        case TechTransferStep::Op::Max: {
            const int n = SafetyOf(plr, st.from);

            if (n <= st.above) {
                break;
            }

            const double cmp = st.compareDivisor > 0.0 ? st.compareDivisor
                               : st.divisor;
            const bool greater = st.compareDouble
                                 ? (double)(n - st.offset) / cmp > (double)acc
                                 : (float)(n - st.offset) / (float)cmp > acc;

            if (greater) {
                acc = (float)(n - st.offset) / (float)st.divisor;
            }

            break;
        }
        }
    }

    return (int)(tt.base + acc);
}

} // namespace


/**
 * Set the starting Safety (and Base) of a program that has just been
 * started, from the Safety of related programs, per the catalog's
 * tech_transfer data.
 *
 * \param plr  0 for the USA, 1 for the USSR.
 * \param category  the EquipmentIndex (PROBE_HARDWARE, etc.)
 * \param unit  the program index within the category.
 * \param mode  the player's gradual formula or the AI's threshold rules.
 *   Programs without data for the mode keep their Safety.
 */
void ApplyTechTransfer(int plr, int category, int unit, TechTransferMode mode)
{
    Equipment& program = HardwareProgram(plr, category, unit);
    const HardwareEntry* entry = CatalogEntryForEquipment(program);

    if (entry != nullptr) {
        const TechTransfer& tt = entry->techTransfer;
        // All inputs are read before the program's Safety is changed.
        int result = program.Safety;

        if (mode == TechTransferMode::Gradual) {
            if (tt.hasGradual) {
                result = GradualTransfer(plr, tt);

                for (const TechTransferRule& rule : tt.overrides) {
                    if (RuleHolds(plr, rule)) {
                        result = rule.safety;
                    }
                }
            }
        } else {
            for (const TechTransferRule& rule : tt.threshold) {
                if (RuleHolds(plr, rule)) {
                    result = rule.safety;
                }
            }
        }

        program.Safety = result;
    }

    program.Base = program.Safety;
}


/** Catalog entry of a program, or nullptr if it is not in the catalog. */
const HardwareEntry* HardwareInfo(const Equipment& program)
{
    return CatalogEntryForEquipment(program);
}


bool HasRole(const Equipment& program, HardwareRole role)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr && e->role == role;
}


/** Whether the program is an orbital, lunar or interplanetary probe. */
bool IsProbe(const Equipment& program)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr
           && (e->role == HardwareRole::OrbitalProbe
               || e->role == HardwareRole::LunarProbe
               || e->role == HardwareRole::InterplanetaryProbe);
}


/** Whether the program is the kicker used for unmanned probe missions. */
bool IsProbeKicker(const Equipment& program)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr && e->role == HardwareRole::Kicker && e->probeKicker;
}


/** Whether the program is a capsule that flies direct ascent landings. */
bool IsDirectAscentCapsule(const Equipment& program)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr && e->directAscent;
}


/** Whether the program is a capsule with a crew of one. */
bool IsOneManCapsule(const Equipment& program)
{
    const HardwareEntry* e = CatalogEntryForEquipment(program);
    return e != nullptr && e->role == HardwareRole::Capsule && e->crew == 1;
}


/**
 * Crew capacity of an astronaut program.
 *
 * \param program  0 for unassigned, otherwise the program number
 *   (Manned index + 1, see ProgramToMannedIndex()).
 */
int ProgramCrewCapacity(int program)
{
    if (program <= 0) {
        return 0;
    }

    // Crew sizes are the same for both sides; take the first player's.
    const HardwareEntry* e = HardwareEntryAt(0, MANNED_HARDWARE,
                                             ProgramToMannedIndex(program));
    return e == nullptr ? 0 : e->crew;
}
