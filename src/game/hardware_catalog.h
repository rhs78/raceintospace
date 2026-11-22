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

/** One step of the "gradual" technology transfer formula. */
struct TechTransferStep {
    enum class Op {
        Set,  ///< if value > above (and > greaterThan): tt = (value - offset) / divisor
        Max,  ///< if value > above and (value - offset) / compareDivisor > tt: tt = (value - offset) / divisor
        Cap   ///< if tt > capMax: tt = capTo
    };

    Op op = Op::Set;
    std::string from;         /**< id of the program whose Safety is read */
    std::string greaterThan;  /**< Set: value must also exceed this program's */
    int above = 0;
    int offset = 0;
    double divisor = 1.0;
    double compareDivisor = 0.0;    /**< Max: 0 means divisor */
    bool compareDouble = false;     /**< Max: compare in double precision */
    float capMax = 0.0f;
    float capTo = 0.0f;
};

/**
 * "When any program of every group has Safety >= atLeast, the result is
 * safety."  Groups are OR-ed inside, AND-ed between.
 */
struct TechTransferRule {
    std::vector<std::vector<std::string>> when;
    int atLeast = 75;
    int safety = 0;
};

/**
 * Starting safety of a program that is started when related programs
 * already exist.  Two variants exist: the player's gradual formula and
 * the AI's threshold rules.  Applied by ApplyTechTransfer() (hardware.cpp).
 */
struct TechTransfer {
    bool hasGradual = false;
    int base = 0;                           /**< Safety = base + tt */
    std::vector<TechTransferStep> steps;
    std::vector<TechTransferRule> overrides; /**< applied after, last match wins */
    std::vector<TechTransferRule> threshold; /**< AI: in order, last match wins */
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
    // Capsule mission limits; -1 means no limit (see CapsuleEligibility()).
    int maxFutureDuration = -1;    /**< highest Future[].Duration level */
    int maxPlanDays = -1;          /**< highest mission plan Days level */
    int maxMinEquipment = -1;      /**< highest mStr::mEq of a mission */
    bool soyuzLL = false;          /**< may fly the Soyuz lunar landing */
    std::string reach;          /**< "" if not specified */
    bool canDock = false;
    bool directAscent = false;
    bool acceptsBoosters = false;
    bool landerNeedsDockingModule = false;
    std::vector<std::string> compatibleLanders;
    std::vector<std::string> compatibleKickers;
    /** Kickers offered for "capsule + kicker" missions (VAB code 0x81). */
    std::vector<std::string> kickersWithCapsule;
    /** Kickers offered on the second pad of joint missions (VAB 0x89). */
    std::vector<std::string> kickersSecondPad;
    bool probeKicker = false;       /**< kicker used for unmanned probes */
    TechTransfer techTransfer;
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

/**
 * The prestige category (Prestige_* value) of the first flight of a
 * program, from its prestige_first field, or -1 if it has none.
 */
int PrestigeFirstCategory(const HardwareEntry& entry);

#endif // HARDWARE_CATALOG_H
