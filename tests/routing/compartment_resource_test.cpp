#include "routing/resources/compartment_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// 3 compartments (frozen=0, fresh=1, ambient=2), 5 clients.
static std::vector<CompartmentResource::ClientInfo> make_info_3comp() {
    return {
        {0, 10},  // client 0: frozen, demand 10
        {1, 5},   // client 1: fresh, demand 5
        {2, 8},   // client 2: ambient, demand 8
        {0, 15},  // client 3: frozen, demand 15
        {1, 3},   // client 4: fresh, demand 3
    };
}

// ===========================================================================
//  Init tests
// ===========================================================================

TEST_CASE("CompartmentResource::init creates single-client state", "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);
    REQUIRE(s0.num_compartments() == 3);
    CHECK(s0.loads[0] == 10);  // frozen
    CHECK(s0.loads[1] == 0);
    CHECK(s0.loads[2] == 0);

    auto s2 = CompartmentResource::init(info, 2, 3);
    CHECK(s2.loads[0] == 0);
    CHECK(s2.loads[1] == 0);
    CHECK(s2.loads[2] == 8);  // ambient
}

TEST_CASE("CompartmentResource::init_depot creates zero-load state", "[compartment_resource]") {
    auto s = CompartmentResource::init_depot(3);
    REQUIRE(s.num_compartments() == 3);
    CHECK(s.loads[0] == 0);
    CHECK(s.loads[1] == 0);
    CHECK(s.loads[2] == 0);
}

TEST_CASE("CompartmentResource::init for unassigned client", "[compartment_resource]") {
    std::vector<CompartmentResource::ClientInfo> info = {
        {-1, 0},  // client 0: no compartment
    };

    auto s = CompartmentResource::init(info, 0, 2);
    CHECK(s.loads[0] == 0);
    CHECK(s.loads[1] == 0);
}

// ===========================================================================
//  Merge tests
// ===========================================================================

TEST_CASE("CompartmentResource::merge combines per-compartment loads", "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);  // frozen=10
    auto s3 = CompartmentResource::init(info, 3, 3);  // frozen=15

    auto merged = CompartmentResource::merge(s0, s3);
    CHECK(merged.loads[0] == 25);  // 10 + 15 in frozen
    CHECK(merged.loads[1] == 0);
    CHECK(merged.loads[2] == 0);
}

TEST_CASE("CompartmentResource::merge across compartments", "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);  // frozen=10
    auto s1 = CompartmentResource::init(info, 1, 3);  // fresh=5
    auto s2 = CompartmentResource::init(info, 2, 3);  // ambient=8

    auto m01 = CompartmentResource::merge(s0, s1);
    auto m012 = CompartmentResource::merge(m01, s2);

    CHECK(m012.loads[0] == 10);
    CHECK(m012.loads[1] == 5);
    CHECK(m012.loads[2] == 8);
}

TEST_CASE("CompartmentResource::merge_reverse equals merge", "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);
    auto s1 = CompartmentResource::init(info, 1, 3);

    auto fwd = CompartmentResource::merge(s0, s1);
    auto rev = CompartmentResource::merge_reverse(s0, s1);

    CHECK(fwd.loads[0] == rev.loads[0]);
    CHECK(fwd.loads[1] == rev.loads[1]);
    CHECK(fwd.loads[2] == rev.loads[2]);
}

TEST_CASE("CompartmentResource::merge with depot", "[compartment_resource]") {
    auto info = make_info_3comp();

    auto depot = CompartmentResource::init_depot(3);
    auto s0 = CompartmentResource::init(info, 0, 3);

    auto merged = CompartmentResource::merge(depot, s0);
    CHECK(merged.loads[0] == 10);
    CHECK(merged.loads[1] == 0);
    CHECK(merged.loads[2] == 0);
}

// ===========================================================================
//  Excess tests
// ===========================================================================

TEST_CASE("CompartmentResource::excess with all compartments within capacity",
          "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);  // frozen=10
    auto s1 = CompartmentResource::init(info, 1, 3);  // fresh=5
    auto merged = CompartmentResource::merge(s0, s1);

    std::vector<int> caps = {20, 10, 15};  // generous capacity
    CHECK(CompartmentResource::excess(merged, caps) == 0);
}

TEST_CASE("CompartmentResource::excess with one compartment over capacity",
          "[compartment_resource]") {
    auto info = make_info_3comp();

    auto s0 = CompartmentResource::init(info, 0, 3);  // frozen=10
    auto s3 = CompartmentResource::init(info, 3, 3);  // frozen=15
    auto merged = CompartmentResource::merge(s0, s3);
    // frozen=25, fresh=0, ambient=0

    std::vector<int> caps = {20, 10, 15};
    CHECK(CompartmentResource::excess(merged, caps) == 5);  // 25 - 20 = 5
}

TEST_CASE("CompartmentResource::excess with multiple compartments over", "[compartment_resource]") {
    auto info = make_info_3comp();

    // Build route: clients 0,1,2,3,4
    auto s0 = CompartmentResource::init(info, 0, 3);
    auto s1 = CompartmentResource::init(info, 1, 3);
    auto s2 = CompartmentResource::init(info, 2, 3);
    auto s3 = CompartmentResource::init(info, 3, 3);
    auto s4 = CompartmentResource::init(info, 4, 3);

    auto m =
        CompartmentResource::merge(CompartmentResource::merge(CompartmentResource::merge(s0, s1),
                                                              CompartmentResource::merge(s2, s3)),
                                   s4);
    // frozen=25, fresh=8, ambient=8

    std::vector<int> caps = {20, 5, 5};
    // frozen excess: 25-20=5, fresh excess: 8-5=3, ambient excess: 8-5=3
    CHECK(CompartmentResource::excess(m, caps) == 11);
}

TEST_CASE("CompartmentResource::excess with empty route", "[compartment_resource]") {
    auto depot = CompartmentResource::init_depot(3);
    std::vector<int> caps = {20, 10, 15};
    CHECK(CompartmentResource::excess(depot, caps) == 0);
}
