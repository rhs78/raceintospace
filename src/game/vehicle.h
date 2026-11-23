#ifndef VEHICLE_H
#define VEHICLE_H

#include <list>
#include <string>
#include <vector>

#include "data.h"

/** A launch vehicle option: a rocket of the player's Rocket list, with or
 *  without the boosters. */
struct VehicleOption {
    int rocket;      /**< position in the player's Rocket list */
    bool boosters;   /**< flown with the boosters */
};

/**
 * All launch vehicles a player can assemble, in the order they are offered:
 * first every launcher of the Rocket list in list order, then, in list
 * order, the launchers that accept boosters combined with the boosters
 * (only if the player has a booster program).  For the classic hardware:
 * Atlas, Titan, Saturn, Nova, Atlas+B, Titan+B, Saturn+B.
 */
std::vector<VehicleOption> LaunchVehicleOptions(int player);

/**
 * This class functions as an interface between the player's data
 * structures and the VAB.
 */
class Vehicle
{
public:
    Vehicle(int player, int rocketIndex, bool hasBoosters = false);
    void assignTo(int pad);
    int available() const;
    bool damaged() const;
    bool delayed() const;
    /** Position of the rocket in the player's Rocket list. */
    int rocketIndex() const
    {
        return mIndex;
    };
    bool hasBoosters() const
    {
        return mSecondary != nullptr;
    };
    int sprite() const;
    std::string name() const;
    std::list<Equipment*> needed();
    void release();
    int safety() const;
    int MaxRD() const;
    bool started() const;
    int thrust() const;
    bool translunar() const;
private:
    int mIndex;
    int mPlayer;
    Equipment* mPrimary;
    Equipment* mSecondary;

    void reserve();
};

#endif // VEHICLE_H
