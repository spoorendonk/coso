#include <catch2/catch_test_macros.hpp>

#include "routing/resources/type_incompatibility.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build instances with typed clients.
// ---------------------------------------------------------------------------

/// 1 depot, 4 clients with types 0, 1, 0, 2.  Single vehicle type.
static ProblemData make_typed_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}, .client_type = 0});   // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .client_type = 1});   // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}, .client_type = 0});   // client 2
    b.add_client({40.0, 0.0}, {.demand = {1}, .client_type = 2});   // client 3

    return b.build(0);
}

/// Instance with some clients having no type (-1).
static ProblemData make_mixed_type_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}, .client_type = 0});    // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .client_type = -1});   // client 1 (no type)
    b.add_client({30.0, 0.0}, {.demand = {1}, .client_type = 1});    // client 2

    return b.build(0);
}

// ===========================================================================
//  TypeIncompatibilityMatrix tests
// ===========================================================================

TEST_CASE("TypeIncompatibilityMatrix: default is all compatible",
          "[type_incompatibility]")
{
    TypeIncompatibilityMatrix m(3);

    CHECK_FALSE(m.incompatible(0, 1));
    CHECK_FALSE(m.incompatible(0, 2));
    CHECK_FALSE(m.incompatible(1, 2));
}

TEST_CASE("TypeIncompatibilityMatrix: set_incompatible is symmetric",
          "[type_incompatibility]")
{
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 2);

    CHECK(m.incompatible(0, 2));
    CHECK(m.incompatible(2, 0));
    CHECK_FALSE(m.incompatible(0, 1));
    CHECK_FALSE(m.incompatible(1, 2));
}

TEST_CASE("TypeIncompatibilityMatrix: multiple incompatible pairs",
          "[type_incompatibility]")
{
    TypeIncompatibilityMatrix m(4);
    m.set_incompatible(0, 1);
    m.set_incompatible(2, 3);

    CHECK(m.incompatible(0, 1));
    CHECK(m.incompatible(2, 3));
    CHECK_FALSE(m.incompatible(0, 2));
    CHECK_FALSE(m.incompatible(1, 3));
}

// ===========================================================================
//  TypeIncompatibilityResource::init tests
// ===========================================================================

TEST_CASE("TypeIncompatibilityResource::init sets correct type",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    CHECK(s0.has_type(0));
    CHECK_FALSE(s0.has_type(1));
    CHECK_FALSE(s0.has_type(2));

    auto s1 = TypeIncompatibilityResource::init(data, 1, m);
    CHECK_FALSE(s1.has_type(0));
    CHECK(s1.has_type(1));
    CHECK_FALSE(s1.has_type(2));
}

TEST_CASE("TypeIncompatibilityResource::init with no type (-1) sets no bits",
          "[type_incompatibility]")
{
    auto data = make_mixed_type_instance();
    TypeIncompatibilityMatrix m(2);

    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // client_type = -1
    CHECK_FALSE(s1.has_type(0));
    CHECK_FALSE(s1.has_type(1));
}

TEST_CASE("TypeIncompatibilityResource::init_depot has no types",
          "[type_incompatibility]")
{
    TypeIncompatibilityMatrix m(3);
    auto s = TypeIncompatibilityResource::init_depot(m);

    CHECK_FALSE(s.has_type(0));
    CHECK_FALSE(s.has_type(1));
    CHECK_FALSE(s.has_type(2));
}

// ===========================================================================
//  TypeIncompatibilityResource::merge tests
// ===========================================================================

TEST_CASE("TypeIncompatibilityResource::merge unions type sets",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // type 1

    auto merged = TypeIncompatibilityResource::merge(s0, s1);
    CHECK(merged.has_type(0));
    CHECK(merged.has_type(1));
    CHECK_FALSE(merged.has_type(2));
}

TEST_CASE("TypeIncompatibilityResource::merge with duplicate types",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);

    // Clients 0 and 2 both have type 0.
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s2 = TypeIncompatibilityResource::init(data, 2, m);  // type 0

    auto merged = TypeIncompatibilityResource::merge(s0, s2);
    CHECK(merged.has_type(0));
    CHECK_FALSE(merged.has_type(1));
    CHECK_FALSE(merged.has_type(2));
}

TEST_CASE("TypeIncompatibilityResource::merge three clients accumulates all types",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // type 1
    auto s3 = TypeIncompatibilityResource::init(data, 3, m);  // type 2

    auto m01  = TypeIncompatibilityResource::merge(s0, s1);
    auto m013 = TypeIncompatibilityResource::merge(m01, s3);

    CHECK(m013.has_type(0));
    CHECK(m013.has_type(1));
    CHECK(m013.has_type(2));
}

TEST_CASE("TypeIncompatibilityResource::merge with untyped client",
          "[type_incompatibility]")
{
    auto data = make_mixed_type_instance();
    TypeIncompatibilityMatrix m(2);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // no type

    auto merged = TypeIncompatibilityResource::merge(s0, s1);
    CHECK(merged.has_type(0));
    CHECK_FALSE(merged.has_type(1));
}

TEST_CASE("TypeIncompatibilityResource::merge_reverse equals merge",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);

    auto fwd = TypeIncompatibilityResource::merge(s0, s1);
    auto rev = TypeIncompatibilityResource::merge_reverse(s0, s1);

    CHECK(fwd.type_bits == rev.type_bits);
}

// ===========================================================================
//  TypeIncompatibilityResource::excess tests
// ===========================================================================

TEST_CASE("TypeIncompatibilityResource::excess with no incompatibilities",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    // No pairs marked as incompatible.

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);
    auto merged = TypeIncompatibilityResource::merge(s0, s1);

    CHECK(TypeIncompatibilityResource::excess(merged, m) == 0);
}

TEST_CASE("TypeIncompatibilityResource::excess detects one incompatible pair",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);

    // Route with clients of type 0 and type 1.
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // type 1

    auto merged = TypeIncompatibilityResource::merge(s0, s1);
    CHECK(TypeIncompatibilityResource::excess(merged, m) == 1);
}

TEST_CASE("TypeIncompatibilityResource::excess with compatible same-type clients",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);

    // Route with clients 0 and 2: both type 0.
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    auto s2 = TypeIncompatibilityResource::init(data, 2, m);

    auto merged = TypeIncompatibilityResource::merge(s0, s2);
    CHECK(TypeIncompatibilityResource::excess(merged, m) == 0);
}

TEST_CASE("TypeIncompatibilityResource::excess with multiple incompatible pairs",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);
    m.set_incompatible(0, 2);
    m.set_incompatible(1, 2);

    // Route with types 0, 1, and 2: three incompatible pairs.
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // type 1
    auto s3 = TypeIncompatibilityResource::init(data, 3, m);  // type 2

    auto m01  = TypeIncompatibilityResource::merge(s0, s1);
    auto m013 = TypeIncompatibilityResource::merge(m01, s3);

    CHECK(TypeIncompatibilityResource::excess(m013, m) == 3);
}

TEST_CASE("TypeIncompatibilityResource::excess with untyped clients is zero",
          "[type_incompatibility]")
{
    auto data = make_mixed_type_instance();
    TypeIncompatibilityMatrix m(2);
    m.set_incompatible(0, 1);

    // Route: client 0 (type 0) + client 1 (no type).
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // -1

    auto merged = TypeIncompatibilityResource::merge(s0, s1);
    CHECK(TypeIncompatibilityResource::excess(merged, m) == 0);
}

TEST_CASE("TypeIncompatibilityResource::excess on depot-only is zero",
          "[type_incompatibility]")
{
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);

    auto s = TypeIncompatibilityResource::init_depot(m);
    CHECK(TypeIncompatibilityResource::excess(s, m) == 0);
}

TEST_CASE("TypeIncompatibilityResource::excess single client is zero",
          "[type_incompatibility]")
{
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);

    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    CHECK(TypeIncompatibilityResource::excess(s0, m) == 0);
}

// ===========================================================================
//  O(1) move evaluation simulation test
// ===========================================================================

TEST_CASE("TypeIncompatibilityResource: prefix/suffix merge for insert eval",
          "[type_incompatibility]")
{
    // Simulate evaluating the effect of inserting a client into a route
    // using prefix/suffix arrays (as Route does for LoadResource).
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);

    // Route: [client 0 (type 0), client 2 (type 0)] -- no excess.
    auto depot = TypeIncompatibilityResource::init_depot(m);
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);
    auto s2 = TypeIncompatibilityResource::init(data, 2, m);

    auto prefix0 = TypeIncompatibilityResource::merge(depot, s0);
    auto prefix1 = TypeIncompatibilityResource::merge(prefix0, s2);

    CHECK(TypeIncompatibilityResource::excess(prefix1, m) == 0);

    // Evaluate inserting client 1 (type 1) at position 1 (between 0 and 2).
    // prefix: depot + client 0.  suffix: client 2 + depot.
    auto suffix1 = TypeIncompatibilityResource::merge(s2, depot);
    auto client1_state = TypeIncompatibilityResource::init(data, 1, m);

    auto left_plus_new = TypeIncompatibilityResource::merge(prefix0, client1_state);
    auto full = TypeIncompatibilityResource::merge(left_plus_new, suffix1);

    // Route would be [type 0, type 1, type 0]: pair (0,1) is incompatible.
    CHECK(TypeIncompatibilityResource::excess(full, m) == 1);
}

TEST_CASE("TypeIncompatibilityResource: partial incompatibility",
          "[type_incompatibility]")
{
    // Only some pairs are incompatible; verify partial detection.
    auto data = make_typed_instance();
    TypeIncompatibilityMatrix m(3);
    m.set_incompatible(0, 1);
    // (0,2) and (1,2) are compatible.

    // Route with types 0, 1, 2.
    auto s0 = TypeIncompatibilityResource::init(data, 0, m);  // type 0
    auto s1 = TypeIncompatibilityResource::init(data, 1, m);  // type 1
    auto s3 = TypeIncompatibilityResource::init(data, 3, m);  // type 2

    auto m01  = TypeIncompatibilityResource::merge(s0, s1);
    auto m013 = TypeIncompatibilityResource::merge(m01, s3);

    // Only pair (0,1) is incompatible.
    CHECK(TypeIncompatibilityResource::excess(m013, m) == 1);
}
