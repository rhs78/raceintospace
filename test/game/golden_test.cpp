/**
 * Golden reference tests, produced from the code BEFORE the hardware
 * rework (stage 4b-4e).  They pin down:
 *
 *  - vab_payloads.txt        BuildVAB() payload combinations
 *  - capsule_eligibility.txt CapsuleEligibility() (crew.cpp rules)
 *  - tech_transfer.txt       ApplyTechTransfer() (rdplex.cpp BuyUnit)
 *
 * The reference files live in test/data/golden/.  To regenerate them
 * (only when a behaviour change is intended) run the tests with the
 * environment variable UPDATE_GOLDEN=1.
 */
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "game/data.h"
#include "game/fs.h"
#include "game/game_main.h"
#include "game/hardware.h"
#include "game/mission_util.h"
#include "game/options.h"
#include "game/rdplex.h"
#include "game/serialize.h"
#include "game/vab.h"


namespace
{

std::unique_ptr<Players> LoadUrast()
{
    std::unique_ptr<Players> players(new Players());
    std::memset(players.get(), 0, sizeof(Players));
    std::string path = locate_file("urast.json", FT_DATA);
    BOOST_REQUIRE(!path.empty());
    DESERIALIZE_JSON_FILE(players, path);
    return players;
}

/** Temporarily replaces the global Data and AI[]. */
struct DataScope {
    Players* savedData;
    char savedAI[2];
    std::unique_ptr<Players> players;

    explicit DataScope(std::unique_ptr<Players> p)
        : savedData(Data), players(std::move(p))
    {
        savedAI[0] = AI[0];
        savedAI[1] = AI[1];
        Data = players.get();
    }

    ~DataScope()
    {
        Data = savedData;
        AI[0] = savedAI[0];
        AI[1] = savedAI[1];
    }
};

bool UpdateMode()
{
    const char* v = std::getenv("UPDATE_GOLDEN");
    return v != nullptr && v[0] != '\0' && v[0] != '0';
}

/** Compare generated text with the golden file (or rewrite the file). */
void CheckGolden(const std::string& name, const std::string& actual)
{
    const std::string path = std::string(TEST_GOLDEN_DIR) + "/" + name;

    if (UpdateMode()) {
        std::ofstream out(path, std::ios::binary);
        BOOST_REQUIRE_MESSAGE(out.good(), "cannot write " << path);
        out << actual;
        return;
    }

    std::ifstream in(path, std::ios::binary);
    BOOST_REQUIRE_MESSAGE(in.good(), "missing golden file " << path
                          << " (generate with UPDATE_GOLDEN=1)");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string expected = ss.str();

    if (expected == actual) {
        return;
    }

    // Report the first differing line.
    std::istringstream e(expected), a(actual);
    std::string el, al;
    int line = 0;

    while (true) {
        bool he = static_cast<bool>(std::getline(e, el));
        bool ha = static_cast<bool>(std::getline(a, al));
        line++;

        if (!he && !ha) {
            break;
        }

        if (he != ha || el != al) {
            BOOST_FAIL("golden mismatch in " << name << " line " << line
                       << "\n  expected: " << (he ? el : "<eof>")
                       << "\n  actual:   " << (ha ? al : "<eof>"));
        }
    }

    BOOST_FAIL("golden mismatch in " << name);
}

Equipment& Hw(Players& d, int plr, int cat, int idx)
{
    switch (cat) {
    case PROBE_HARDWARE:
        return d.P[plr].Probe[idx];

    case ROCKET_HARDWARE:
        return d.P[plr].Rocket[idx];

    case MANNED_HARDWARE:
        return d.P[plr].Manned[idx];

    default:
        return d.P[plr].Misc[idx];
    }
}

// ---------------------------------------------------------------------
// VAB payloads
// ---------------------------------------------------------------------

std::string VasSnapshot()
{
    std::ostringstream o;
    o << "n=" << VASqty;

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 4; j++) {
            const VInfo& v = VAS[i][j];

            if (std::strcmp(v.name, "NONE") == 0 && v.dex == -1
                && v.img == -1 && v.qty == 0) {
                continue;
            }

            o << " [" << i << "," << j << ":" << v.name
              << " q" << int(v.qty) << " a" << int(v.ac)
              << " sf" << int(v.sf) << " wt" << v.wt
              << " dex" << int(v.dex) << " img" << int(v.img)
              << " dmg" << int(v.dmg) << " rd" << v.MaxRD << "]";
        }
    }

    return o.str();
}

/** The urast start state with varied stock/damage so fields differ. */
std::unique_ptr<Players> VabTestData()
{
    std::unique_ptr<Players> players = LoadUrast();

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            for (int i = 0; i < HardwareCount(c); i++) {
                Equipment& eq = Hw(*players, plr, c, i);
                eq.Num = (c * 7 + i + plr) % 4 - 1;
                eq.Spok = i % 3;
                eq.Damage = ((c + i) % 4 == 0) ? 10 : 0;
                eq.Safety = 40 + 5 * ((c + i + plr) % 9);
                eq.MSF = 30 + 3 * i;
            }
        }
    }

    return players;
}


// ---------------------------------------------------------------------
// Capsule eligibility: the ORIGINAL inline rules of crew.cpp
// HardRequest() (manned == false) and SecondHard() (manned == true),
// with all programs treated as purchased.
// ---------------------------------------------------------------------

std::array<bool, 5> OrigEligibility(int plr, int mis, int duration, bool manned)
{
    int pr[5] = {1, 1, 1, 1, 1};
    const mStr plan = GetMissionPlan(mis);

    if (!manned) {
        if (plan.Lun == 1 || plan.Doc == 1 || plan.mEq > 1 || plan.Days > 1 ||
            duration > 2) {
            pr[0] = 0;
        }

        if (duration > 5 || plan.Days > 5) {
            pr[1] = 0;
        }

        if (duration > 3) {
            pr[3] = 0;
        }

        if (mis == Mission_DirectAscent_LL) {
            pr[0] = pr[1] = pr[2] = pr[3] = 0;
        }

        if (mis == Mission_Soyuz_LL && plr == 1) {
            pr[0] = pr[1] = pr[3] = pr[4] = 0;
        }

        if (plan.Doc == 1) {
            pr[4] = 0;
        }
    } else {
        if (plan.Lun == 1 || plan.Doc == 1 || plan.mEq > 1 || plan.Days > 2
            || duration > 2) {
            pr[0] = 0;
        }

        if (duration > 5 || plan.Days > 5) {
            pr[1] = 0;
        }

        if (duration > 4) {
            pr[3] = 0;
        }

        if (mis == Mission_DirectAscent_LL) {
            pr[0] = pr[1] = pr[2] = pr[3] = 0;
        }

        if (plan.Doc == 1) {
            pr[4] = 0;
        }

        if (mis == Mission_Soyuz_LL && plr == 1) {
            pr[0] = pr[1] = pr[3] = pr[4] = 0;
        }
    }

    return {pr[0] != 0, pr[1] != 0, pr[2] != 0, pr[3] != 0, pr[4] != 0};
}

// ---------------------------------------------------------------------
// Tech transfer
// ---------------------------------------------------------------------

struct Lcg {
    uint32_t state;
    explicit Lcg(uint32_t seed) : state(seed) {}

    uint32_t next()
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }
};

} // namespace


BOOST_AUTO_TEST_SUITE(golden_suite)

BOOST_AUTO_TEST_CASE(golden_vab_payloads)
{
    DataScope scope(VabTestData());
    std::ostringstream out;
    out << "# BuildVAB(plr, mission, ty=1, part, prog) payloads, AI[plr]=0;\n"
        << "# 'ai1' lines are listed only where AI[plr]=1 differs.\n";

    for (const mStr& m : GetMissionData()) {
        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            for (int part = 0; part < 2; part++) {
                for (int prog = 0; prog < MANNED_PROGRAM_COUNT; prog++) {
                    AI[plr] = 0;
                    BuildVAB(plr, m.Index, 1, part, prog);
                    const std::string normal = VasSnapshot();

                    AI[plr] = 1;
                    BuildVAB(plr, m.Index, 1, part, prog);
                    const std::string ai = VasSnapshot();
                    AI[plr] = 0;

                    out << "m" << int(m.Index) << " plr" << plr
                        << " part" << part << " prog" << prog << ": "
                        << normal << "\n";

                    if (ai != normal) {
                        out << "m" << int(m.Index) << " plr" << plr
                            << " part" << part << " prog" << prog
                            << " ai1: " << ai << "\n";
                    }
                }
            }
        }
    }

    CheckGolden("vab_payloads.txt", out.str());
}

BOOST_AUTO_TEST_CASE(golden_capsule_eligibility)
{
    std::ostringstream out;
    out << "# CapsuleEligibility(plr, mission, futureDuration, choice): "
        << "bits for Manned[0..4]\n"
        << "# choice u = unmanned (HardRequest), m = manned (SecondHard)\n";

    for (const mStr& m : GetMissionData()) {
        for (int plr = 0; plr < NUM_PLAYERS; plr++) {
            for (int dur = 0; dur <= 6; dur++) {
                out << "m" << int(m.Index) << " plr" << plr << " dur" << dur;

                for (CapsuleChoice choice : {CapsuleChoice::UnmannedMission,
                                             CapsuleChoice::MannedMission
                                            }) {
                    out << " "
                        << (choice == CapsuleChoice::UnmannedMission ? 'u' : 'm')
                        << "=";

                    const auto got = CapsuleEligibility(plr, m.Index, dur, choice);
                    const auto want = OrigEligibility(
                                          plr, m.Index, dur,
                                          choice == CapsuleChoice::MannedMission);
                    BOOST_CHECK_MESSAGE(got == want,
                                        "eligibility differs from original rules"
                                        " m" << int(m.Index) << " plr" << plr
                                        << " dur" << dur);

                    for (bool ok : got) {
                        out << (ok ? '1' : '0');
                    }
                }

                out << "\n";
            }
        }
    }

    CheckGolden("capsule_eligibility.txt", out.str());
}

BOOST_AUTO_TEST_CASE(golden_tech_transfer)
{
    // For every program, apply the tech transfer for a number of
    // deterministic pseudo-random sibling safety combinations and record
    // the resulting Safety/Base of all programs of the category.
    DataScope scope(LoadUrast());
    std::ostringstream out;
    out << "# ApplyTechTransfer(category, unit, plr): input = Safety of "
        << "all programs of the category, output = Safety/Base after.\n";

    const int STRUCTURED = 6;
    const int RANDOM = 40;

    for (int plr = 0; plr < NUM_PLAYERS; plr++) {
        for (int c = PROBE_HARDWARE; c <= MISC_HARDWARE; c++) {
            const int n = (c == MISC_HARDWARE) ? MISC_HW_DOCKING_MODULE + 1
                          : HardwareCount(c);

            for (int unit = 0; unit < n; unit++) {
                Lcg rng(1000 * plr + 100 * c + unit + 1);

                for (int sample = 0; sample < STRUCTURED + RANDOM; sample++) {
                    std::vector<int> in(n);

                    for (int i = 0; i < n; i++) {
                        switch (sample) {
                        case 0:
                            in[i] = 0;
                            break;

                        case 1:
                            in[i] = 40;
                            break;

                        case 2:
                            in[i] = 75;
                            break;

                        case 3:
                            in[i] = 98;
                            break;

                        case 4:
                            in[i] = (i == unit) ? 0 : 98;
                            break;

                        case 5:
                            in[i] = (i == unit) ? 98 : 0;
                            break;

                        default:
                            in[i] = rng.next() % 101;
                        }
                    }

                    for (int i = 0; i < n; i++) {
                        Equipment& eq = Hw(*scope.players, plr, c, i);
                        eq.Safety = in[i];
                        eq.Base = in[i];
                    }

                    ApplyTechTransfer(c, unit, plr);

                    out << "plr" << plr << " c" << c << " u" << unit << " in";

                    for (int v : in) {
                        out << " " << v;
                    }

                    out << " out";

                    for (int i = 0; i < n; i++) {
                        const Equipment& eq = Hw(*scope.players, plr, c, i);
                        out << " " << eq.Safety << "/" << int(eq.Base);
                    }

                    out << "\n";
                }
            }
        }
    }

    CheckGolden("tech_transfer.txt", out.str());
}

BOOST_AUTO_TEST_SUITE_END()
