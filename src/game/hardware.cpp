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

#include <cassert>
#include <stdexcept>

#include "game_main.h"
#include "hardware_catalog.h"
#include "macros.h"
#include "mission_util.h"
#include "options.h"


/**
 * Number of hardware programs in use for an equipment category.
 *
 * \param equipmentIndex  an EquipmentIndex value.
 * \return  3 (Probe), 5 (Rocket), 7 (Manned) or 6 (Misc).
 * \throws std::invalid_argument  if equipmentIndex is not valid.
 */
int HardwareCount(int equipmentIndex)
{
    switch (equipmentIndex) {
    case PROBE_HARDWARE:
        return PROBE_HW_COUNT;

    case ROCKET_HARDWARE:
        return ROCKET_HW_COUNT;

    case MANNED_HARDWARE:
        return MANNED_HW_COUNT;

    case MISC_HARDWARE:
        return MISC_HW_COUNT;

    default:
        throw std::invalid_argument(
            "Error in function HardwareCount: "
            "argument is not part of the EquipmentIndex enum");
    }
}


/**
 * Encode a launch vehicle for MissionType.Hard[Mission_PrimaryBooster].
 *
 * The value is the rocket's index + 1, so that 0 (or less) can mean
 * "no rocket assigned", plus ROCKET_HW_BOOSTERS if boosters are fitted.
 *
 * \param rocketIndex  an EquipRocketIndex, excluding the Boosters.
 * \param boosters  true if the rocket is fitted with boosters.
 */
int EncodeRocket(int rocketIndex, bool boosters)
{
    return rocketIndex + 1 + (boosters ? ROCKET_HW_BOOSTERS : 0);
}


/**
 * Rocket index from a MissionType.Hard[Mission_PrimaryBooster] value.
 *
 * \param hardValue  an encoded rocket, see EncodeRocket().
 * \return  the index into the Rocket array (never the Boosters), or -1
 *   if hardValue means no rocket is assigned (<= 0).
 */
int DecodeRocketIndex(int hardValue)
{
    if (hardValue <= 0) {
        return -1;
    }

    return (hardValue - 1) % ROCKET_HW_BOOSTERS;
}


/**
 * Whether an encoded rocket (see EncodeRocket()) has boosters fitted.
 * False for the "no rocket" value.
 */
bool HasBoosters(int hardValue)
{
    return hardValue > 0 && (hardValue - 1) >= ROCKET_HW_BOOSTERS;
}


/**
 * Whether a MissionType.Hard[Mission_Probe_DM] value selects the
 * Docking Module rather than a Probe.
 */
bool IsDockingModule(int hardValue)
{
    return hardValue == HARD_DOCKING_MODULE;
}


/**
 * Resolve the overloaded MissionType.Hard[Mission_Probe_DM] value.
 *
 * \param player  0 for the USA, 1 for the USSR
 * \param hardValue  HARD_DOCKING_MODULE for the Docking Module
 *   (Misc[MISC_HW_DOCKING_MODULE]), otherwise a Probe index.
 */
Equipment &ProbeOrDockingModule(int player, int hardValue)
{
    assert(player >= 0 && player < NUM_PLAYERS);

    if (IsDockingModule(hardValue)) {
        return Data->P[player].Misc[MISC_HW_DOCKING_MODULE];
    }

    return Data->P[player].Probe[hardValue];
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
    const HardwareCatalog& catalog = GetHardwareCatalog();

    // Capsules of this side that can fly the Soyuz lunar landing.
    bool sideHasSoyuzLL = false;

    for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
        const HardwareEntry* e = catalog.FindByLegacy(plr, MANNED_HARDWARE, i);

        if (e != nullptr && e->soyuzLL) {
            sideHasSoyuzLL = true;
        }
    }

    for (int i = 0; i < MANNED_PROGRAM_COUNT; i++) {
        const HardwareEntry* e = catalog.FindByLegacy(plr, MANNED_HARDWARE, i);

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
    assert(program >= 0 && program < 7);
    assert(type >= 0 && type < 4);

    switch (type) {
    case PROBE_HARDWARE:
        return Data->P[player].Probe[program];
        break;

    case ROCKET_HARDWARE:
        return Data->P[player].Rocket[program];
        break;

    case MANNED_HARDWARE:
        return Data->P[player].Manned[program];
        break;

    case MISC_HARDWARE:
        return Data->P[player].Misc[program];
        break;

    default:
        throw std::invalid_argument(
            "Error in argument to HardwareProgram: type must be part of "
            "the EquipmentIndex enum");
        break;
    };
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
    const HardwareEntry* e = GetHardwareCatalog().FindById(id);

    if (e == nullptr) {
        throw std::invalid_argument("tech transfer: unknown program " + id);
    }

    return HardwareProgram(plr, e->legacyCat, e->legacyIdx).Safety;
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
    const HardwareEntry* entry =
        GetHardwareCatalog().FindByLegacy(plr, category, unit);

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

    const HardwareEntry* e = GetHardwareCatalog().FindByLegacyAnySide(
                                 MANNED_HARDWARE, ProgramToMannedIndex(program));
    return e == nullptr ? 0 : e->crew;
}
