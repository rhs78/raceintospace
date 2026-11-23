#ifndef VEHICLE_SELECTOR_H
#define VEHICLE_SELECTOR_H

#include <stdexcept>
#include <vector>

#include "hardware.h"
#include "vehicle.h"


/**
 * This class cycles through the available launch vehicle options,
 * composed of a rocket and optional boosters (see LaunchVehicleOptions()):
 * every launcher of the player's Rocket list, then the launchers that
 * accept boosters combined with the boosters.
 */
class VehicleSelector
{
public:
    VehicleSelector(int plr) :  mIndex(0), mPlayer(plr),
        mOptions(LaunchVehicleOptions(plr)) {};
    Vehicle start();
    Vehicle next();
    /** Number of vehicle options. */
    int count() const { return int(mOptions.size()); }
private:
    int mIndex;
    const int mPlayer;
    const std::vector<VehicleOption> mOptions;
};


inline Vehicle VehicleSelector::start()
{
    mIndex = 0;
    return next();
}


inline Vehicle VehicleSelector::next()
{
    if (mOptions.empty()) {
        throw std::logic_error("The player has no launch vehicle.");
    }

    // By having mIndex track the next vehicle to be used, rather than the
    // current one, there is no need to initially call start()
    const VehicleOption& option = mOptions[mIndex];

    if (++mIndex >= int(mOptions.size())) {
        mIndex = 0;
    }

    return Vehicle(mPlayer, option.rocket, option.boosters);
}


#endif // VEHICLE_SELECTOR_H
