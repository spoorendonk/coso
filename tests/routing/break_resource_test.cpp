#include "routing/resources/break_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build instances for testing break resource.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients on a line, each with service time 30.
/// Distances (and durations) are Euclidean so travel between adjacent
/// clients = 10.  Depot-to-first and last-to-depot = 10.
static ProblemData make_break_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {100}});

    // Clients on a line: (10,0), (20,0), (30,0), (40,0).
    // Service time 30 each.
    b.add_client({10.0, 0.0}, {.demand = {1}, .service = 30});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .service = 30});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}, .service = 30});  // client 2
    b.add_client({40.0, 0.0}, {.demand = {1}, .service = 30});  // client 3

    return b.build(0);
}

/// Instance with varying service times for more nuanced tests.
static ProblemData make_varied_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}, .service = 20});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .service = 50});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}, .service = 10});  // client 2

    return b.build(0);
}

// ===========================================================================
//  Init tests
// ===========================================================================

TEST_CASE("BreakResource::init creates state with service time as driving", "[break_resource]") {
    auto data = make_break_instance();
    BreakResource::Rule rule{.max_driving = 100, .break_duration = 15};

    auto s = BreakResource::init(data, 0, rule);
    CHECK(s.driving == 30);
    CHECK(s.driving_rev == 30);
    CHECK(s.duration == 30);
    CHECK(s.breaks == 0);
    CHECK(s.excess == 0);
    CHECK(s.can_break == true);
}

TEST_CASE("BreakResource::init_depot creates empty state", "[break_resource]") {
    auto data = make_break_instance();
    BreakResource::Rule rule{.max_driving = 100, .break_duration = 15};

    auto s = BreakResource::init_depot(data, rule);
    CHECK(s.driving == 0);
    CHECK(s.driving_rev == 0);
    CHECK(s.duration == 0);
    CHECK(s.breaks == 0);
    CHECK(s.excess == 0);
    CHECK(s.can_break == true);
}

TEST_CASE("BreakResource::init with service exceeding max_driving", "[break_resource]") {
    auto data = make_break_instance();
    // Max driving is 20, but service is 30.
    // Init does NOT charge excess; it is detected by excess() on the
    // final state via the trailing driving segment.
    BreakResource::Rule rule{.max_driving = 20, .break_duration = 15};

    auto s = BreakResource::init(data, 0, rule);
    CHECK(s.driving == 30);
    CHECK(s.excess == 0);
    // But excess() should detect it.
    CHECK(BreakResource::excess(s, rule) == 10);
}

// ===========================================================================
//  Merge tests
// ===========================================================================

TEST_CASE("BreakResource::merge two clients within limit", "[break_resource]") {
    auto data = make_break_instance();
    // Service = 30 each, travel = 10 between adjacent clients.
    // Total driving: 30 + 10 + 30 = 70.
    BreakResource::Rule rule{.max_driving = 100, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    auto merged = BreakResource::merge(s0, s1, 10, rule);
    CHECK(merged.duration == 70);  // 30 + 10 + 30
    CHECK(merged.driving == 30);   // right's driving (from last break)
    CHECK(merged.excess == 0);
    CHECK(merged.breaks == 0);
}

TEST_CASE("BreakResource::merge accumulates driving with no break rule", "[break_resource]") {
    auto data = make_break_instance();
    // No break rule (max_driving = 0 means unlimited).
    BreakResource::Rule rule{.max_driving = 0, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    auto merged = BreakResource::merge(s0, s1, 10, rule);
    CHECK(merged.driving == 70);  // accumulates: 30 + 10 + 30
    CHECK(merged.excess == 0);
}

TEST_CASE("BreakResource::merge detects excess when bridge exceeds limit", "[break_resource]") {
    auto data = make_break_instance();
    // Max driving = 50. Client 0 service=30, travel=10 -> bridge=40.
    // bridge + right's driving would be 40 + 30 = 70 > 50.
    // But right can_break=true, so bridge (40) < 50, no excess yet.
    // After break at right, driving = right.driving = 30, which is < 50.
    BreakResource::Rule rule{.max_driving = 50, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    auto merged = BreakResource::merge(s0, s1, 10, rule);
    CHECK(merged.excess == 0);  // bridge = 40 < 50, fits
    CHECK(merged.driving == 30);

    // Now check full route excess: driving=30 < 50, so no violation.
    CHECK(BreakResource::excess(merged, rule) == 0);
}

TEST_CASE("BreakResource::merge with bridge exceeding max_driving", "[break_resource]") {
    auto data = make_break_instance();
    // Max driving = 35. Client 0 service=30, travel=10 -> bridge=40 > 35.
    // Excess = 40 - 35 = 5. Break forced at right. driving = 30.
    BreakResource::Rule rule{.max_driving = 35, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    auto merged = BreakResource::merge(s0, s1, 10, rule);
    CHECK(merged.excess == 5);    // bridge 40 - 35
    CHECK(merged.breaks == 1);    // forced break
    CHECK(merged.driving == 30);  // reset after break
}

TEST_CASE("BreakResource::merge three clients chain", "[break_resource]") {
    auto data = make_break_instance();
    // Each service = 30, travel = 10 between adjacent.
    // Max driving = 60. Break duration = 15.
    //
    // Client 0: driving=30
    // Merge(0,1) with travel=10: bridge=40 < 60. driving=30. No excess.
    // Merge(0+1, 2) with travel=10: bridge=40 < 60. driving=30. No excess.
    BreakResource::Rule rule{.max_driving = 60, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);
    auto s2 = BreakResource::init(data, 2, rule);

    auto m01 = BreakResource::merge(s0, s1, 10, rule);
    auto m012 = BreakResource::merge(m01, s2, 10, rule);

    CHECK(m012.excess == 0);
    CHECK(m012.driving == 30);
    CHECK(BreakResource::excess(m012, rule) == 0);
}

TEST_CASE("BreakResource::merge three clients with violations", "[break_resource]") {
    auto data = make_break_instance();
    // Max driving = 25. Each service=30 already exceeds limit.
    // Client 0: driving=30, excess = 30-25 = 5.
    // Client 1: driving=30, excess = 5.
    // Client 2: driving=30, excess = 5.
    //
    // Merge(0,1) travel=10: bridge=40 > 25. excess += 40-25=15, break forced.
    //   Total excess so far: 5 + 5 + 15 = 25. driving=30.
    // Merge(01,2) travel=10: bridge=40 > 25. excess += 40-25=15, break forced.
    //   Total excess: 25 + 5 + 15 = 45. driving=30.
    // Final excess: 45 + max(0, 30-25) = 45 + 5 = 50.
    BreakResource::Rule rule{.max_driving = 25, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);
    auto s2 = BreakResource::init(data, 2, rule);

    auto m01 = BreakResource::merge(s0, s1, 10, rule);
    auto m012 = BreakResource::merge(m01, s2, 10, rule);

    // Bridge excesses: 15 + 15 = 30 (no init excess, detected by excess())
    CHECK(m012.excess == 30);

    // Plus final segment: 30 - 25 = 5
    CHECK(BreakResource::excess(m012, rule) == 35);
}

// ===========================================================================
//  Excess computation tests
// ===========================================================================

TEST_CASE("BreakResource::excess with no rule returns zero", "[break_resource]") {
    BreakResource::Rule rule{.max_driving = 0, .break_duration = 15};
    BreakResource::State s{.driving = 999, .excess = 0};

    CHECK(BreakResource::excess(s, rule) == 0);
}

TEST_CASE("BreakResource::excess adds final segment violation", "[break_resource]") {
    BreakResource::Rule rule{.max_driving = 50, .break_duration = 15};
    BreakResource::State s;
    s.driving = 70;  // 20 over limit
    s.excess = 10;   // already accumulated

    CHECK(BreakResource::excess(s, rule) == 30);  // 10 + 20
}

TEST_CASE("BreakResource::excess with driving within limit", "[break_resource]") {
    BreakResource::Rule rule{.max_driving = 50, .break_duration = 15};
    BreakResource::State s;
    s.driving = 40;
    s.excess = 5;

    CHECK(BreakResource::excess(s, rule) == 5);  // only accumulated excess
}

// ===========================================================================
//  Merge reverse tests
// ===========================================================================

TEST_CASE("BreakResource::merge_reverse swaps driving directions", "[break_resource]") {
    auto data = make_varied_instance();
    // Client 0: service=20, Client 1: service=50.
    BreakResource::Rule rule{.max_driving = 100, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    // Forward merge: left=s0, right=s1 reversed.
    auto merged = BreakResource::merge_reverse(s0, s1, 10, rule);
    CHECK(merged.duration == 80);  // 20 + 10 + 50
}

// ===========================================================================
//  Integration-style tests: building full routes
// ===========================================================================

TEST_CASE("BreakResource: full route feasible with generous limit", "[break_resource]") {
    auto data = make_break_instance();
    BreakResource::Rule rule{.max_driving = 500, .break_duration = 15};

    // Build full route: depot -> c0 -> c1 -> c2 -> c3 -> depot.
    // Travel: 10 each hop. Service: 30 each.
    // Total driving: 30+10+30+10+30+10+30 = 150.
    auto depot = BreakResource::init_depot(data, rule);
    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);
    auto s2 = BreakResource::init(data, 2, rule);
    auto s3 = BreakResource::init(data, 3, rule);

    auto m = BreakResource::merge(depot, s0, 10, rule);
    m = BreakResource::merge(m, s1, 10, rule);
    m = BreakResource::merge(m, s2, 10, rule);
    m = BreakResource::merge(m, s3, 10, rule);
    auto full = BreakResource::merge(m, depot, 40, rule);  // c3(40,0)->depot(0,0)=40

    CHECK(full.duration == 200);  // 4*30 + 10+10+10+10+40 = 120+80 = 200
    CHECK(BreakResource::excess(full, rule) == 0);
}

TEST_CASE("BreakResource: full route with tight limit causes excess", "[break_resource]") {
    auto data = make_break_instance();
    // Max 45 driving. Service=30, travel=10. Bridge after first client = 40.
    // After c0: driving=30 < 45. Merge with c1 (travel=10): bridge=40 < 45.
    // driving=30. Merge with c2 (travel=10): bridge=40 < 45.
    // driving=30. Merge with c3 (travel=10): bridge=40 < 45.
    // Final driving=30 < 45.
    // All good at 45!
    BreakResource::Rule rule{.max_driving = 45, .break_duration = 15};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);
    auto s2 = BreakResource::init(data, 2, rule);
    auto s3 = BreakResource::init(data, 3, rule);

    auto m = BreakResource::merge(s0, s1, 10, rule);
    m = BreakResource::merge(m, s2, 10, rule);
    m = BreakResource::merge(m, s3, 10, rule);

    CHECK(BreakResource::excess(m, rule) == 0);

    // Now with max_driving = 35. bridge=40 > 35 each time.
    // Excess per bridge: 40-35 = 5. Three bridges -> 15 accumulated.
    // Final driving = 30 < 35.
    BreakResource::Rule tight{.max_driving = 35, .break_duration = 15};

    auto t0 = BreakResource::init(data, 0, tight);
    auto t1 = BreakResource::init(data, 1, tight);
    auto t2 = BreakResource::init(data, 2, tight);
    auto t3 = BreakResource::init(data, 3, tight);

    auto tm = BreakResource::merge(t0, t1, 10, tight);
    tm = BreakResource::merge(tm, t2, 10, tight);
    tm = BreakResource::merge(tm, t3, 10, tight);

    CHECK(tm.excess == 15);                         // 3 bridges * 5 excess each
    CHECK(BreakResource::excess(tm, tight) == 15);  // final driving=30 < 35
}

TEST_CASE("BreakResource: varied service times with breaks", "[break_resource]") {
    auto data = make_varied_instance();
    // Client 0: service=20, Client 1: service=50, Client 2: service=10.
    // Travel = 10 between adjacent.
    // Max driving = 40, break_duration = 10.
    //
    // c0: driving=20. Merge with c1 (travel=10): bridge=30 < 40. driving=50.
    // But wait — right.can_break=true, so driving = right.driving = 50.
    // Hmm, 50 > 40 but that's the final segment driving.
    // excess from state: 0. excess() adds 50-40 = 10. Total = 10.
    BreakResource::Rule rule{.max_driving = 40, .break_duration = 10};

    auto s0 = BreakResource::init(data, 0, rule);
    auto s1 = BreakResource::init(data, 1, rule);

    auto m = BreakResource::merge(s0, s1, 10, rule);
    CHECK(m.excess == 0);                         // bridge=30 < 40, no bridge excess
    CHECK(m.driving == 50);                       // right.driving = client 1 service
    CHECK(BreakResource::excess(m, rule) == 10);  // final: 50 - 40 = 10

    // Add client 2 (service=10, travel=10): bridge = 50+10 = 60 > 40.
    // Excess += 60 - 40 = 20, break forced. driving = 10.
    auto s2 = BreakResource::init(data, 2, rule);
    auto m2 = BreakResource::merge(m, s2, 10, rule);
    CHECK(m2.excess == 20);                        // bridge 60 - 40
    CHECK(m2.driving == 10);                       // reset after break
    CHECK(BreakResource::excess(m2, rule) == 20);  // 10 < 40, no final excess
}
