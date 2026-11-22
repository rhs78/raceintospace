#ifndef HARDWARE_H
#define HARDWARE_H

#include <array>

#include "data.h"

/**
 * Number of hardware programs in use per category (the arrays in
 * PlayerData are padded to 7 entries, but only these are used).
 */
const int PROBE_HW_COUNT = 3;
const int ROCKET_HW_COUNT = 5;   // includes the Boosters
const int MANNED_HW_COUNT = 7;   // includes the Lunar Modules
const int MISC_HW_COUNT = 6;

/**
 * MissionType.Hard[Mission_Probe_DM] is overloaded: values 0..2 select
 * a Probe program, this value selects the Docking Module (Misc).
 */
const int HARD_DOCKING_MODULE = 4;

/** Number of crewed programs (Manned[0..4] - capsules/minishuttle). */
const int MANNED_PROGRAM_COUNT = 5;

int HardwareCount(int equipmentIndex);
int EncodeRocket(int rocketIndex, bool boosters);
int DecodeRocketIndex(int hardValue);
bool HasBoosters(int hardValue);
bool IsDockingModule(int hardValue);
Equipment &ProbeOrDockingModule(int player, int hardValue);
int ProgramToMannedIndex(int program);
int MannedIndexToProgram(int mannedIndex);

/** Which crew.cpp selection screen the capsule rules are evaluated for. */
enum class CapsuleChoice {
    UnmannedMission,  ///< HardRequest(): capsule for a future unmanned mission
    MannedMission     ///< SecondHard(): capsule for a future manned mission
};

std::array<bool, MANNED_PROGRAM_COUNT>
CapsuleEligibility(int plr, int missionCode, int futureDuration,
                   CapsuleChoice choice);

int CrewSize(const Equipment& program);
Equipment &HardwareProgram(int player, int type, int program);
MissionHardwareType HardwareType(const Equipment& program);
int RocketBoosterSafety(int safetyRocket, int safetyBooster);
int RocketMaxRD(int MaxRDRocket, int MaxRDBooster);

#endif // HARDWARE_H
