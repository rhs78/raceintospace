#ifndef HARDWARE_CATALOG_H
#define HARDWARE_CATALOG_H

/**
 * \file hardware_catalog.h
 *
 * Descriptive hardware catalog (stage 4a), loaded from
 * data/gamedata/hardware.json.  See doc/fork/hardware-katalog.md.
 *
 * In this step the catalog only describes the 21 original programs per
 * side; game behaviour is unchanged.
 */

#include <map>
#include <string>
#include <vector>

#include "data.h"

/** Hardware role, replaces index checks in the game code. */
enum class HardwareRole {
    OrbitalProbe,
    LunarProbe,
    InterplanetaryProbe,
    Launcher,
    Booster,
    Capsule,
    Minishuttle,
    Lander,
    Kicker,
    Eva,
    DockingModule,
    PhotoRecon,
};

struct HardwareImages {
    std::string rd;             /**< research screen image */
    std::string liftoff;        /**< liftoff screen image */
    int vabSprite = -1;         /**< VabSprite index, -1 if none */
    int vabSpriteBoosters = -1; /**< VabSprite index with boosters, -1 if none */
    std::string aprog;          /**< astronaut program image, "" if none */
    int patchBase = -1;         /**< first patch image index, -1 if none */
    int mhist = -1;             /**< mission history image, -1 if none */
};

/** Mission limits of a capsule; -1 means no limit. */
struct HardwareLimits {
    int maxFutureDurationManned = -1;
    int maxFutureDurationUnmanned = -1;
    int maxPlanDaysManned = -1;
    int maxPlanDaysUnmanned = -1;
};

struct HardwareEntry {
    std::string id;             /**< e.g. "US.ATLAS" */
    std::string side;           /**< "US" or "SV" */
    int player = 0;             /**< 0 for US, 1 for SV */
    std::string category;       /**< probe, rocket, manned, misc */
    HardwareRole role = HardwareRole::Launcher;
    std::string name;           /**< default program name */
    std::string typeLabel;      /**< text shown after the name ("ROCKET") */
    bool labelReplacesName = false;
    int crew = 0;
    int unitWeight = 0;
    int maxPayload = 0;
    int durationDays = 0;
    char durationLevel = 0;     /**< 'A'..'F', 0 if not limited/applicable */
    bool hasLimits = false;
    HardwareLimits limits;
    std::string reach;          /**< "" if not specified */
    bool canDock = false;
    bool directAscent = false;
    bool acceptsBoosters = false;
    bool landerNeedsDockingModule = false;
    std::vector<std::string> compatibleLanders;
    std::vector<std::string> compatibleKickers;
    std::string prestigeFirst;  /**< "" if none, else e.g. "OnePerson" */
    std::string animId;         /**< two characters, e.g. "C2" */
    HardwareImages images;
    std::string helpId;         /**< e.g. "i20103" */
    int legacyCat = 0;          /**< EquipmentIndex */
    int legacyIdx = 0;          /**< index within the legacy array */

    /** The mission slot this hardware fills. */
    MissionHardwareType missionType() const;
};

class HardwareCatalog
{
public:
    /**
     * Parse and validate a catalog document.
     * \throws std::runtime_error  on parse or validation errors.
     */
    static HardwareCatalog FromJsonText(const std::string& text);

    const std::vector<HardwareEntry>& entries() const { return mEntries; }

    const HardwareEntry* FindById(const std::string& id) const;

    /** Lookup by player (0/1) and legacy (EquipmentIndex, index). */
    const HardwareEntry* FindByLegacy(int player, int cat, int idx) const;

    /**
     * Lookup by legacy slot on whichever side has it. Only for data that
     * is identical for both sides (role, crew size).
     */
    const HardwareEntry* FindByLegacyAnySide(int cat, int idx) const;

    std::vector<const HardwareEntry*> FindByRole(HardwareRole role,
                                                 int player = -1) const;

private:
    void Validate() const;

    std::vector<HardwareEntry> mEntries;
    std::map<std::string, size_t> mById;
};

/**
 * The catalog loaded from hardware.json (lazily, once).
 * \throws std::runtime_error  if the file is missing or invalid.
 */
const HardwareCatalog& GetHardwareCatalog();

/**
 * Catalog entry for an Equipment, looked up via its ID characters
 * (category letter and index). Equipment carries no side, so this is only
 * meaningful for side-independent data. Returns nullptr if not found.
 */
const HardwareEntry* CatalogEntryForEquipment(const Equipment& program);

HardwareRole HardwareRoleFromString(const std::string& s);

#endif // HARDWARE_CATALOG_H
