/* Copyright (C) 2020
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "vehicle.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <sstream>

#include "data.h"
#include "game_main.h"
#include "hardware.h"
#include "hardware_catalog.h"


std::vector<VehicleOption> LaunchVehicleOptions(int player)
{
    std::vector<VehicleOption> options;
    const std::vector<Equipment>& rockets =
        HardwareList(Data->P[player], ROCKET_HARDWARE);
    const bool haveBoosters = BoosterIndex(player) >= 0;

    for (size_t i = 0; i < rockets.size(); i++) {
        if (HasRole(rockets[i], HardwareRole::Launcher)) {
            options.push_back({int(i), false});
        }
    }

    if (haveBoosters) {
        for (size_t i = 0; i < rockets.size(); i++) {
            const HardwareEntry* entry = HardwareInfo(rockets[i]);

            if (entry != nullptr && entry->role == HardwareRole::Launcher
                && entry->acceptsBoosters) {
                options.push_back({int(i), true});
            }
        }
    }

    return options;
}


/**
 * \param player  player index.
 * \param rocketIndex  position of the rocket in the player's Rocket list;
 *   the entry must be a launcher (not the boosters).
 * \param hasBoosters  fit the player's boosters; only for rockets that
 *   accept boosters (catalog accepts_boosters).
 */
Vehicle::Vehicle(int player, int rocketIndex, bool hasBoosters)
    : mPrimary(nullptr), mSecondary(nullptr)
{
    if (player < 0 || player >= NUM_PLAYERS) {
        std::stringstream err;
        err << "player = " << player << " out of range,"
            << " must be 0 <= player < " << NUM_PLAYERS << '\n';
        throw std::out_of_range(err.str());
    }

    const int rockets = HardwareCount(player, ROCKET_HARDWARE);

    if (rocketIndex < 0 || rocketIndex >= rockets) {
        std::stringstream err;
        err << "rocketIndex = " << rocketIndex << " out of range,"
            << " must be 0 <= rocketIndex < " << rockets << '\n';
        throw std::out_of_range(err.str());
    }

    mPlayer = player;
    mIndex = rocketIndex;
    mPrimary = &HardwareProgram(player, ROCKET_HARDWARE, rocketIndex);

    assert(mPrimary);

    if (!HasRole(*mPrimary, HardwareRole::Launcher)) {
        throw std::logic_error("The rocket program is not a launcher.");
    }

    if (hasBoosters) {
        const HardwareEntry* entry = HardwareInfo(*mPrimary);

        if (entry == nullptr || !entry->acceptsBoosters) {
            throw std::logic_error("This rocket does not support boosters.");
        }

        const int boosters = BoosterIndex(player);

        if (boosters < 0) {
            throw std::logic_error("The player has no booster program.");
        }

        mSecondary = &HardwareProgram(player, ROCKET_HARDWARE, boosters);
    }
}


/**
 * Assign the launch vehicle hardware to a Mission launching this turn.
 *
 * \param pad  pad index to player's current Mission.
 * \throws out_of_range  if pad is not in range [0, MAX_LAUNCHPADS).
 * \throws logic_error  if no mission is assigned or it already has
 *   a launch vehicle.
 */
void Vehicle::assignTo(int pad)
{
    // TODO: Replace this with a Mission accessor.
    if (pad < 0 || pad >= MAX_LAUNCHPADS) {
        std::stringstream err;
        err << "pad = " << pad << " out of range,"
            << " must be 0 <= pad < " << MAX_LAUNCHPADS << '\n';
        throw std::out_of_range(err.str());
    }

    if (Data->P[mPlayer].LaunchFacility[pad] != LAUNCHPAD_OPERATIONAL) {
        std::stringstream err;
        err << "Pad " << 'A' + pad << " is unavailable for a mission.";
        throw std::logic_error(err.str());
    }

    MissionType &mission = Data->P[mPlayer].Mission[pad];

    if (mission.MissionCode == Mission_None) {
        std::stringstream err;
        err << "No mission on pad " << 'A' + pad
            << ". Cannot assign launch vehicle.";
        throw std::logic_error(err.str());
    }

    if (HasLaunchVehicle(mission)) {
        std::stringstream err;
        err << "Cannot assign launch vehicle to pad " << 'A' + pad
            << ": Hardware already assigned.";
        throw std::logic_error(err.str());
    }

    reserve();
    mission.Hard[Mission_Rocket] = mIndex;
    mission.Booster = (mSecondary != nullptr);
}


/**
 * How many instances of the launch vehicle are available for mission
 * assignment.
 *
 * \return  the number of launch vehicles minus those already committed.
 */
int Vehicle::available() const
{
    if (mSecondary) {
        return std::min(std::max(mPrimary->Num - mPrimary->Spok, 0),
                        std::max(mSecondary->Num - mSecondary->Spok, 0));
    } else {
        return std::max(mPrimary->Num - mPrimary->Spok, 0);
    }
}


bool Vehicle::damaged() const
{
    return mPrimary->Damage || (mSecondary && mSecondary->Damage);
}


bool Vehicle::delayed() const
{
    return mPrimary->Delay != 0 || (mSecondary && mSecondary->Delay != 0);
}


std::string Vehicle::name() const
{
    std::string value(mPrimary->Name, sizeof(mPrimary->Name));
    return (mSecondary ? "B/" + value : value);
}


/**
 * Identify the hardware components left to create an unassigned
 * launch vehicle instance.
 */
std::list<Equipment *> Vehicle::needed()
{
    std::list<Equipment *> parts;

    // TODO: Throw exception or just return empty?
    if (! started()) {
        return parts;
    }

    if (mPrimary->Num <= mPrimary->Spok) {
        parts.push_back(mPrimary);
    }

    if (mSecondary != nullptr && mSecondary->Num <= mSecondary->Spok) {
        parts.push_back(mSecondary);
    }

    return parts;
}


void Vehicle::release()
{
    bool reserved = mPrimary->Spok > 0;

    if (mSecondary) {
        reserved = reserved && (mSecondary->Spok > 0);
    }

    if (! reserved) {
        throw std::logic_error(
            "No tasked vehicles instances to be release");
    }

    mPrimary->Spok--;

    if (mSecondary) {
        mSecondary->Spok--;
    }
}


void Vehicle::reserve()
{
    if (available() <= 0) {
        throw std::logic_error(
            "No available vehicle instances to reserve");
    }

    mPrimary->Spok++;

    if (mSecondary) {
        mSecondary->Spok++;
    }
}


int Vehicle::safety() const
{
    if (mSecondary) {
        return RocketBoosterSafety(mPrimary->Safety, mSecondary->Safety);
    } else {
        return mPrimary->Safety;
    }
}


int Vehicle::MaxRD() const
{
    if (mSecondary) {
        return RocketMaxRD(mPrimary->MaxRD, mSecondary->MaxRD);
    } else {
        return mPrimary->MaxRD;
    }
}


bool Vehicle::started() const
{
    return mPrimary->Num >= 0 && ((!mSecondary) || mSecondary->Num >= 0);
}


int Vehicle::thrust() const
{
    return mPrimary->MaxPay + ((mSecondary) ? mSecondary->MaxPay : 0);
}


/**
 * Is the launch vehicle able to carry a payload beyond Low Earth Orbit?
 * (catalog reach of the rocket: translunar or interplanetary)
 */
bool Vehicle::translunar() const
{
    const HardwareEntry* entry = HardwareInfo(*mPrimary);
    return entry != nullptr
           && (entry->reach == "translunar" || entry->reach == "interplanetary");
}


/**
 * The VAB image of the launch vehicle, per the catalog: the rocket's
 * sprite, or the one of the rocket with boosters.
 */
int Vehicle::sprite() const
{
    const HardwareEntry* entry = HardwareInfo(*mPrimary);

    if (entry == nullptr) {
        return 0;
    }

    const int sprite = mSecondary ? entry->images.vabSpriteBoosters
                       : entry->images.vabSprite;
    return sprite >= 0 ? sprite : 0;
}
