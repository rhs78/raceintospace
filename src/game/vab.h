#ifndef VAB_H
#define VAB_H

#include <array>
#include <vector>

#include "data.h"

class Vehicle;

void VAB(char plr);
void BuildVAB(char plr, char mis, char ty, char pa, char pr);
void ReserveHardware(int plr, int pad, int payload, Vehicle& rocket);

/** The payload configurations of the mission being assembled: rows
 *  1..VASqty, four components each (see vab.cpp). */
extern std::vector<std::array<VInfo, 4>> VAS;
extern int VASqty;

#endif // VAB_H
