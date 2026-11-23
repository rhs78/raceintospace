#ifndef HARDWARE_H
#define HARDWARE_H

#include <array>
#include <vector>

#include "data.h"
#include "hardware_catalog.h"

/**
 * Number of hardware programs per category in the CLASSIC scenario.  Only
 * for code and tests that are specific to the classic hardware; other code
 * must use HardwareCount() (list sizes of the game state).
 */
const int PROBE_HW_COUNT = 3;
const int ROCKET_HW_COUNT = 5;   // includes the Boosters
const int MANNED_HW_COUNT = 7;   // includes the Lunar Modules
const int MISC_HW_COUNT = 6;

/**
 * Number of crewed programs (Manned[0..4] - capsules/minishuttle) in the
 * CLASSIC scenario.  See ProgramCount() for the general value.
 */
const int MANNED_PROGRAM_COUNT = 5;

int HardwareCount(int equipmentIndex);
int HardwareCount(int player, int equipmentIndex);
std::vector<Equipment>& HardwareList(BuzzData& player, int equipmentIndex);
const std::vector<Equipment>& HardwareList(const BuzzData& player,
                                           int equipmentIndex);
int ProgramCount(const BuzzData& player);
int ProgramCount(int player);
void SizeHardwareTables(BuzzData& player);
void ValidateHardwareState(const Players& data);
const HardwareEntry* HardwareEntryAt(int player, int equipmentIndex, int unit);
bool IsResearchable(const Equipment& program);
int FirstResearchableUnit(const BuzzData& player, int equipmentIndex);
int CycleResearchableUnit(const BuzzData& player, int equipmentIndex,
                          int unit, int step);
int HardwareListIndex(int player, const HardwareEntry& entry);

// Mission hardware (stage 4d): list positions, no encodings.
std::vector<int> HardwareIndicesByRole(int player, HardwareRole role);
int HardwareIndexByRole(int player, HardwareRole role);
int DockingModuleIndex(int player);
int BoosterIndex(int player);
int EvaSuitsIndex(int player);
void ClearMissionHardware(MissionType& mission);
bool HasLaunchVehicle(const MissionType& mission);
Equipment* MissionEquipment(int player, const MissionType& mission,
                            MissionHardwareType slot);
int ProbeImageSlot(int player, int probeIndex);
int ProgramToMannedIndex(int program);
int MannedIndexToProgram(int mannedIndex);

std::array<bool, MANNED_PROGRAM_COUNT>
CapsuleEligibility(int plr, int missionCode, int futureDuration);

/** Which technology transfer variant of the catalog to apply. */
enum class TechTransferMode {
    Gradual,   ///< the player's formula (rdplex.cpp BuyUnit)
    Threshold  ///< the AI's threshold rules (aipur.cpp GenPur)
};

void ApplyTechTransfer(int plr, int category, int unit,
                       TechTransferMode mode);

const HardwareEntry* HardwareInfo(const Equipment& program);
bool HasRole(const Equipment& program, HardwareRole role);
bool IsProbe(const Equipment& program);
bool IsProbeKicker(const Equipment& program);
bool IsDirectAscentCapsule(const Equipment& program);
bool IsOneManCapsule(const Equipment& program);
int ProgramCrewCapacity(int program);

int CrewSize(const Equipment& program);
Equipment &HardwareProgram(int player, int type, int program);
MissionHardwareType HardwareType(const Equipment& program);
int RocketBoosterSafety(int safetyRocket, int safetyBooster);
int RocketMaxRD(int MaxRDRocket, int MaxRDBooster);

#endif // HARDWARE_H
