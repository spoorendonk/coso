#include "routing/resources/precedence_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a pickup-delivery instance.
// ---------------------------------------------------------------------------

/// 1 depot, 4 clients: pickup pair (0->1), pickup pair (2->3).
/// Client 0 = pickup for request 0, client 1 = delivery for request 0.
/// Client 2 = pickup for request 1, client 3 = delivery for request 1.
static ProblemData make_pd_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {1}});  // client 0 (pickup req 0)
    b.add_client({20.0, 0.0}, {.demand = {1}});  // client 1 (delivery req 0)
    b.add_client({30.0, 0.0}, {.demand = {1}});  // client 2 (pickup req 1)
    b.add_client({40.0, 0.0}, {.demand = {1}});  // client 3 (delivery req 1)

    b.add_request(0, 1);  // request 0: pickup=0, delivery=1
    b.add_request(2, 3);  // request 1: pickup=2, delivery=3

    return b.build(0);
}

/// 1 depot, 3 clients: only one pickup-delivery pair (0->2), client 1 is normal.
static ProblemData make_mixed_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {1}});  // client 0 (pickup req 0)
    b.add_client({20.0, 0.0}, {.demand = {1}});  // client 1 (normal)
    b.add_client({30.0, 0.0}, {.demand = {1}});  // client 2 (delivery req 0)

    b.add_request(0, 2);  // request 0: pickup=0, delivery=2

    return b.build(0);
}

// ---------------------------------------------------------------------------
//  Tests
// ---------------------------------------------------------------------------

TEST_CASE("PrecedenceResource: init for pickup client", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init(lookup, 0);  // pickup for req 0
    REQUIRE(s.active.size() == 1);
    REQUIRE(s.active[0] == 0);
    REQUIRE(s.needed.empty());
    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: init for delivery client", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init(lookup, 1);  // delivery for req 0
    REQUIRE(s.active.empty());
    REQUIRE(s.needed.size() == 1);
    REQUIRE(s.needed[0] == 0);
    REQUIRE(PrecedenceResource::excess(s) == 1);  // violation: no pickup
}

TEST_CASE("PrecedenceResource: init for normal client", "[precedence]") {
    auto data = make_mixed_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init(lookup, 1);  // normal client
    REQUIRE(s.active.empty());
    REQUIRE(s.needed.empty());
    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: init_depot", "[precedence]") {
    auto s = PrecedenceResource::init_depot();
    REQUIRE(s.active.empty());
    REQUIRE(s.needed.empty());
    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: merge pickup then delivery (feasible)", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s0 = PrecedenceResource::init(lookup, 0);  // pickup req 0
    auto s1 = PrecedenceResource::init(lookup, 1);  // delivery req 0

    auto merged = PrecedenceResource::merge(s0, s1);
    REQUIRE(PrecedenceResource::excess(merged) == 0);
    REQUIRE(merged.active.empty());
    REQUIRE(merged.needed.empty());
}

TEST_CASE("PrecedenceResource: merge delivery then pickup (infeasible)", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s1 = PrecedenceResource::init(lookup, 1);  // delivery req 0
    auto s0 = PrecedenceResource::init(lookup, 0);  // pickup req 0

    auto merged = PrecedenceResource::merge(s1, s0);
    REQUIRE(PrecedenceResource::excess(merged) == 1);
    REQUIRE(merged.needed.size() == 1);  // delivery still unresolved
    REQUIRE(merged.active.size() == 1);  // pickup still active
}

TEST_CASE("PrecedenceResource: full feasible route [p0, d0, p1, d1]", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 2));
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 3));

    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: full feasible interleaved [p0, p1, d0, d1]", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));  // p0
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 2));  // p1
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));  // d0
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 3));  // d1

    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: infeasible route [d0, p0, p1, d1]", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));  // d0 first!
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));  // p0
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 2));  // p1
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 3));  // d1

    REQUIRE(PrecedenceResource::excess(s) == 1);  // 1 violation (d0 before p0)
}

TEST_CASE("PrecedenceResource: two violations [d0, d1, p0, p1]", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));  // d0
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 3));  // d1
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));  // p0
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 2));  // p1

    REQUIRE(PrecedenceResource::excess(s) == 2);
}

TEST_CASE("PrecedenceResource: mixed clients don't cause violations", "[precedence]") {
    auto data = make_mixed_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    // Route: pickup(0), normal(1), delivery(2)
    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));  // pickup
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));  // normal
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 2));  // delivery

    REQUIRE(PrecedenceResource::excess(s) == 0);
}

TEST_CASE("PrecedenceResource: merge subsequences via prefix/suffix", "[precedence]") {
    // Test the O(1) merge pattern: build prefix for [p0, p1], suffix for [d0, d1],
    // then merge them.
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    // Left subsequence: [p0, p1]
    auto left = PrecedenceResource::init(lookup, 0);
    left = PrecedenceResource::merge(left, PrecedenceResource::init(lookup, 2));

    // Right subsequence: [d0, d1]
    auto right = PrecedenceResource::init(lookup, 1);
    right = PrecedenceResource::merge(right, PrecedenceResource::init(lookup, 3));

    // Merge: should be feasible (pickups before deliveries)
    auto full = PrecedenceResource::merge(left, right);
    REQUIRE(PrecedenceResource::excess(full) == 0);
}

TEST_CASE("PrecedenceResource: merge subsequences with violation", "[precedence]") {
    auto data = make_pd_instance();
    auto lookup = PrecedenceResource::build_lookup(data);

    // Left subsequence: [d0, p1]  (delivery 0 without its pickup)
    auto left = PrecedenceResource::init(lookup, 1);
    left = PrecedenceResource::merge(left, PrecedenceResource::init(lookup, 2));

    // Right subsequence: [p0, d1]  (pickup 0 too late; delivery 1 ok)
    auto right = PrecedenceResource::init(lookup, 0);
    right = PrecedenceResource::merge(right, PrecedenceResource::init(lookup, 3));

    auto full = PrecedenceResource::merge(left, right);
    // d0 needed from left but p0 is in right (too late) -> 1 violation
    // d1 in right, p1 in left -> resolved, 0 violations
    REQUIRE(PrecedenceResource::excess(full) == 1);
}

TEST_CASE("PrecedenceResource: no requests means no violations", "[precedence]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});
    b.add_client({10.0, 0.0}, {.demand = {1}});
    b.add_client({20.0, 0.0}, {.demand = {1}});
    auto data = b.build(0);

    auto lookup = PrecedenceResource::build_lookup(data);

    auto s = PrecedenceResource::init_depot();
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 0));
    s = PrecedenceResource::merge(s, PrecedenceResource::init(lookup, 1));

    REQUIRE(PrecedenceResource::excess(s) == 0);
}
