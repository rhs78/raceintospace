/**
 * Tests for the serialization helpers and the Astros class version.
 */
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "game/data.h"
#include "game/serialize.h"


namespace
{

struct VecHolder {
    int16_t v[4];
    std::array<int, 4> a;

    template<class Archive>
    void serialize(Archive& ar)
    {
        ARCHIVE_VECTOR(v, int16_t, 4);
        ARCHIVE_ARRAY(a, int);
    }
};


// Same field names as VecHolder, but with arbitrary lengths.
struct VecSource {
    std::vector<int16_t> v;
    std::vector<int> a;

    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(CEREAL_NVP(v));
        ar(CEREAL_NVP(a));
    }
};

}


BOOST_AUTO_TEST_SUITE(serialize_suite)

BOOST_AUTO_TEST_CASE(astros_race_round_trips)
{
    Astros in = {};
    strcpy(in.Name, "TESTER");
    in.Race = 2;
    in.Sex = 1;
    in.Compat = 7;

    std::stringstream ss;
    {
        cereal::JSONOutputArchive oar(ss);
        oar(in);
    }

    Astros out = {};
    {
        cereal::JSONInputArchive iar(ss);
        iar(out);
    }

    BOOST_TEST((int)out.Race == 2);
    BOOST_TEST((int)out.Sex == 1);
    BOOST_TEST((int)out.Compat == 7);
    BOOST_TEST(std::string(out.Name) == "TESTER");
}

BOOST_AUTO_TEST_CASE(short_arrays_are_zero_filled)
{
    // Write a file whose arrays are shorter than the destination.
    std::stringstream ss;
    {
        cereal::JSONOutputArchive oar(ss);
        VecSource src{{1, 2}, {3}};
        oar(src);
    }

    VecHolder h;
    for (auto& x : h.v) {
        x = 99;
    }
    h.a.fill(99);
    {
        cereal::JSONInputArchive iar(ss);
        iar(h);
    }

    BOOST_TEST(h.v[0] == 1);
    BOOST_TEST(h.v[1] == 2);
    BOOST_TEST(h.v[2] == 0);
    BOOST_TEST(h.v[3] == 0);
    BOOST_TEST(h.a[0] == 3);
    BOOST_TEST(h.a[1] == 0);
    BOOST_TEST(h.a[3] == 0);
}

BOOST_AUTO_TEST_CASE(long_arrays_are_truncated)
{
    std::stringstream ss;
    {
        cereal::JSONOutputArchive oar(ss);
        VecSource src{{1, 2, 3, 4, 5, 6}, {1, 2, 3, 4, 5}};
        oar(src);
    }

    VecHolder h;
    {
        cereal::JSONInputArchive iar(ss);
        iar(h);
    }

    BOOST_TEST(h.v[3] == 4);
    BOOST_TEST(h.a[3] == 4);
}

BOOST_AUTO_TEST_CASE(urast_json_still_loads)
{
    std::unique_ptr<Players> data(new Players());
    std::ifstream is(std::string(TEST_DATADIR) + "/gamedata/urast.json");
    BOOST_REQUIRE(is.good());
    cereal::JSONInputArchive iar(is);
    iar(*data);
    BOOST_TEST(data->StartYear == 57);
    BOOST_TEST(data->P[0].Probe.empty());  // hardware comes from the scenario
}

BOOST_AUTO_TEST_SUITE_END()
