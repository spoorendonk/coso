#include "routing/resources/sync_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// 6 clients, 2 sync groups:
///   Group 0: clients 0,1 (tolerance=10)
///   Group 1: clients 2,3 (tolerance=5)
///   Clients 4,5 are not in any sync group.
static std::vector<SyncResource::SyncGroup> make_sync_groups() {
    return {
        {0, {0, 1}, 10},
        {1, {2, 3}, 5},
    };
}

// ===========================================================================
//  Build lookup tests
// ===========================================================================

TEST_CASE("SyncResource::build_lookup assigns group ids correctly", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    CHECK(lookup[0] == 0);
    CHECK(lookup[1] == 0);
    CHECK(lookup[2] == 1);
    CHECK(lookup[3] == 1);
    CHECK(lookup[4] == -1);
    CHECK(lookup[5] == -1);
}

// ===========================================================================
//  Init tests
// ===========================================================================

TEST_CASE("SyncResource::init for sync group member", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    auto s = SyncResource::init(lookup, 0, 100);
    REQUIRE(s.entries.size() == 1);
    CHECK(s.entries[0].group_id == 0);
    CHECK(s.entries[0].client == 0);
    CHECK(s.entries[0].arrival_time == 100);
}

TEST_CASE("SyncResource::init for non-sync client", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    auto s = SyncResource::init(lookup, 4, 200);
    CHECK(s.entries.empty());
}

TEST_CASE("SyncResource::init_depot creates empty state", "[sync_resource]") {
    auto s = SyncResource::init_depot();
    CHECK(s.entries.empty());
}

// ===========================================================================
//  Merge tests
// ===========================================================================

TEST_CASE("SyncResource::merge concatenates entries", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    auto s0 = SyncResource::init(lookup, 0, 100);
    auto s2 = SyncResource::init(lookup, 2, 200);

    auto merged = SyncResource::merge(s0, s2);
    REQUIRE(merged.entries.size() == 2);
    CHECK(merged.entries[0].group_id == 0);
    CHECK(merged.entries[1].group_id == 1);
}

TEST_CASE("SyncResource::merge with depot", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    auto depot = SyncResource::init_depot();
    auto s0 = SyncResource::init(lookup, 0, 50);

    auto merged = SyncResource::merge(depot, s0);
    REQUIRE(merged.entries.size() == 1);
    CHECK(merged.entries[0].arrival_time == 50);
}

// ===========================================================================
//  Excess tests
// ===========================================================================

TEST_CASE("SyncResource::excess with arrivals within tolerance", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    // Route A visits client 0 at time 100.
    auto route_a = SyncResource::init(lookup, 0, 100);

    // Route B visits client 1 at time 108 (group 0, tolerance 10).
    auto route_b = SyncResource::init(lookup, 1, 108);

    CHECK(SyncResource::excess(route_a, route_b, groups) == 0);
}

TEST_CASE("SyncResource::excess with arrivals beyond tolerance", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    // Route A visits client 0 at time 100.
    auto route_a = SyncResource::init(lookup, 0, 100);

    // Route B visits client 1 at time 115 (group 0, tolerance 10).
    // Diff = 15, excess = 15 - 10 = 5.
    auto route_b = SyncResource::init(lookup, 1, 115);

    CHECK(SyncResource::excess(route_a, route_b, groups) == 5);
}

TEST_CASE("SyncResource::excess with no shared groups", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    // Route A visits client 0 (group 0).
    auto route_a = SyncResource::init(lookup, 0, 100);

    // Route B visits client 2 (group 1).
    auto route_b = SyncResource::init(lookup, 2, 200);

    // No shared groups -> no excess.
    CHECK(SyncResource::excess(route_a, route_b, groups) == 0);
}

TEST_CASE("SyncResource::excess with multiple violations", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    // Route A visits client 0 (group 0) and client 2 (group 1).
    auto sa0 = SyncResource::init(lookup, 0, 100);
    auto sa2 = SyncResource::init(lookup, 2, 200);
    auto route_a = SyncResource::merge(sa0, sa2);

    // Route B visits client 1 (group 0) and client 3 (group 1).
    auto sb1 = SyncResource::init(lookup, 1, 120);  // diff=20, tol=10, ex=10
    auto sb3 = SyncResource::init(lookup, 3, 210);  // diff=10, tol=5,  ex=5
    auto route_b = SyncResource::merge(sb1, sb3);

    CHECK(SyncResource::excess(route_a, route_b, groups) == 15);
}

TEST_CASE("SyncResource single-route excess is always 0", "[sync_resource]") {
    auto groups = make_sync_groups();
    auto lookup = SyncResource::build_lookup(6, groups);

    auto s0 = SyncResource::init(lookup, 0, 100);
    CHECK(SyncResource::excess(s0) == 0);
}
