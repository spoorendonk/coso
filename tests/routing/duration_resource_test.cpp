#include <catch2/catch_test_macros.hpp>

#include "routing/route.h"
#include "routing/resources/duration_resource.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a VRPTW instance for testing.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0) with TW [0, 1000], 4 clients on a line.
/// Duration matrix == distance matrix (speed = 1).
/// Clients have time windows and service times.
static ProblemData make_vrptw_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {.tw = {0, 1000}});
    b.add_vehicle_type(2, {.capacity = {100}});

    // client 0 at (10,0): TW [5, 50], service = 5
    b.add_client({10.0, 0.0}, {.demand = {1}, .tw = {5, 50}, .service = 5});
    // client 1 at (20,0): TW [20, 80], service = 10
    b.add_client({20.0, 0.0}, {.demand = {1}, .tw = {20, 80}, .service = 10});
    // client 2 at (30,0): TW [50, 100], service = 5
    b.add_client({30.0, 0.0}, {.demand = {1}, .tw = {50, 100}, .service = 5});
    // client 3 at (0,10): TW [0, 15], service = 3
    b.add_client({0.0, 10.0}, {.demand = {1}, .tw = {0, 15}, .service = 3});

    return b.build(0);
}

/// Tight time windows that force infeasibility.
static ProblemData make_infeasible_tw_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {.tw = {0, 100}});
    b.add_vehicle_type(1, {.capacity = {100}});

    // client 0 at (10,0): TW [5, 10], service = 5
    // After serving: depart at 10+5 = 15.
    b.add_client({10.0, 0.0}, {.demand = {1}, .tw = {5, 10}, .service = 5});
    // client 1 at (20,0): TW [12, 14], service = 5
    // Arrive at 15 + 10 = 25 > 14 => time warp = 11.
    b.add_client({20.0, 0.0}, {.demand = {1}, .tw = {12, 14}, .service = 5});

    return b.build(0);
}

/// Instance with explicit durations (different from distances).
static ProblemData make_explicit_duration_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {.tw = {0, 200}});
    b.add_vehicle_type(1, {.capacity = {100}});

    // 2 clients
    b.add_client({10.0, 0.0}, {.demand = {1}, .tw = {0, 50}, .service = 5});
    b.add_client({20.0, 0.0}, {.demand = {1}, .tw = {0, 100}, .service = 5});

    // Set explicit durations: all travel takes 3 time units.
    // 3 nodes: depot=0, client0=1, client1=2
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (i != j)
                b.set_duration(0, i, j, 3);

    return b.build(0);
}

// ===========================================================================
//  DurationResource basic tests
// ===========================================================================

TEST_CASE("DurationResource::init creates correct single-client state",
          "[duration_resource]")
{
    auto data = make_vrptw_instance();

    // Client 0: TW [5, 50], service = 5.
    auto s0 = DurationResource::init(data, 0, data.num_depots() + 0);
    CHECK(s0.earliest == 10);    // tw.start + service = 5 + 5
    CHECK(s0.latest == 50);      // tw.end
    CHECK(s0.duration == 5);     // service
    CHECK(s0.time_warp == 0);
    CHECK(s0.first_node == 1);   // depot=0, so client 0 = node 1
    CHECK(s0.last_node == 1);
}

TEST_CASE("DurationResource::init_depot creates depot state",
          "[duration_resource]")
{
    auto data = make_vrptw_instance();
    auto s = DurationResource::init_depot(data, 0);
    CHECK(s.earliest == 0);
    CHECK(s.latest == 1000);
    CHECK(s.duration == 0);
    CHECK(s.time_warp == 0);
}

TEST_CASE("DurationResource::merge two feasible clients",
          "[duration_resource]")
{
    auto data = make_vrptw_instance();

    // depot -> client 0 -> client 1
    // depot at (0,0), TW [0, 1000]
    // client 0 at (10,0), TW [5, 50], service = 5
    // client 1 at (20,0), TW [20, 80], service = 10

    auto depot = DurationResource::init_depot(data, 0);
    auto c0 = DurationResource::init(data, 0, 1);
    auto c1 = DurationResource::init(data, 1, 2);

    // depot -> client 0: travel = dur(0, 0, 1) = 10 (Euclidean distance).
    auto d_c0 = DurationResource::merge(depot, c0, data, 0);
    // Arrival at client 0 = 0 + 10 = 10. TW starts at 5, so no wait.
    // TW ends at 50, 10 <= 50, so no warp.
    // Earliest departure = max(10, 5) + 5 = 15.
    CHECK(d_c0.earliest == 15);
    CHECK(d_c0.time_warp == 0);

    // depot -> client 0 -> client 1: travel from c0 to c1 = 10.
    auto d_c0_c1 = DurationResource::merge(d_c0, c1, data, 0);
    // Arrival at client 1 = 15 + 10 = 25. TW [20, 80], 25 <= 80 so no warp.
    // Earliest departure = max(25, 20) + 10 = 35.
    CHECK(d_c0_c1.earliest == 35);
    CHECK(d_c0_c1.time_warp == 0);
}

TEST_CASE("DurationResource::merge with waiting",
          "[duration_resource]")
{
    auto data = make_vrptw_instance();

    // Go to client 2 first (TW [50, 100]):
    // depot -> client 2: travel = 30. Arrive at 30. TW starts at 50, wait 20.
    auto depot = DurationResource::init_depot(data, 0);
    auto c2 = DurationResource::init(data, 2, 3);

    auto d_c2 = DurationResource::merge(depot, c2, data, 0);
    // Arrival at c2 = 0 + 30 = 30. TW start = 50 (right.earliest - right.duration = 55 - 5 = 50).
    // effective_start = max(30, 50) = 50. Departure = 50 + 5 = 55.
    CHECK(d_c2.earliest == 55);
    CHECK(d_c2.time_warp == 0);
}

TEST_CASE("DurationResource::merge with time warp",
          "[duration_resource]")
{
    auto data = make_infeasible_tw_instance();

    auto depot = DurationResource::init_depot(data, 0);
    auto c0 = DurationResource::init(data, 0, 1);  // TW [5, 10], svc=5
    auto c1 = DurationResource::init(data, 1, 2);  // TW [12, 14], svc=5

    // depot -> c0: travel = 10. Arrive at 10. TW [5, 10], 10 <= 10.
    // Departure = max(10, 5) + 5 = 15.
    auto d_c0 = DurationResource::merge(depot, c0, data, 0);
    CHECK(d_c0.earliest == 15);
    CHECK(d_c0.time_warp == 0);

    // c0 -> c1: travel = 10. Arrive at 15 + 10 = 25. TW [12, 14], 25 > 14.
    // Time warp = 25 - 14 = 11.
    auto d_c0_c1 = DurationResource::merge(d_c0, c1, data, 0);
    CHECK(d_c0_c1.time_warp == 11);
    // Earliest = max(25, 12) + 5 = 30.
    CHECK(d_c0_c1.earliest == 30);
}

TEST_CASE("DurationResource::excess returns time warp",
          "[duration_resource]")
{
    auto data = make_infeasible_tw_instance();

    auto depot = DurationResource::init_depot(data, 0);
    auto c0 = DurationResource::init(data, 0, 1);
    auto c1 = DurationResource::init(data, 1, 2);

    auto d_c0 = DurationResource::merge(depot, c0, data, 0);
    auto full = DurationResource::merge(d_c0, c1, data, 0);

    CHECK(DurationResource::excess(full, data.vehicle_type(0)) == 11);
    CHECK(DurationResource::time_warp(full) == 11);
}

TEST_CASE("DurationResource with explicit durations",
          "[duration_resource]")
{
    auto data = make_explicit_duration_instance();

    auto depot = DurationResource::init_depot(data, 0);
    auto c0 = DurationResource::init(data, 0, 1);
    auto c1 = DurationResource::init(data, 1, 2);

    // depot -> c0: travel = 3 (explicit). TW [0, 50], svc=5.
    // Arrive at 3, depart at max(3, 0) + 5 = 8.
    auto d_c0 = DurationResource::merge(depot, c0, data, 0);
    CHECK(d_c0.earliest == 8);
    CHECK(d_c0.time_warp == 0);

    // c0 -> c1: travel = 3. Arrive at 11. TW [0, 100], svc=5.
    // Depart at max(11, 0) + 5 = 16.
    auto d_c0_c1 = DurationResource::merge(d_c0, c1, data, 0);
    CHECK(d_c0_c1.earliest == 16);
    CHECK(d_c0_c1.time_warp == 0);
}

// ===========================================================================
//  Route duration tracking tests
// ===========================================================================

TEST_CASE("Route: empty route has zero time warp", "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    CHECK(route.time_warp() == 0);
    CHECK(route.tw_feasible());
}

TEST_CASE("Route: feasible VRPTW route has zero time warp",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    // depot -> c0 -> c1: all TW feasible (checked in DurationResource tests).
    route.set_clients({0, 1});

    CHECK(route.time_warp() == 0);
    CHECK(route.tw_feasible());
}

TEST_CASE("Route: infeasible VRPTW route has positive time warp",
          "[route][duration]")
{
    auto data = make_infeasible_tw_instance();
    Route route(data, 0);

    route.set_clients({0, 1});

    CHECK(route.time_warp() > 0);
    CHECK_FALSE(route.tw_feasible());
}

TEST_CASE("Route: eval_insert_time_warp matches actual insert",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    route.set_clients({0, 1});

    // Evaluate inserting client 2 at every position.
    for (int pos = 0; pos <= route.size(); ++pos) {
        int predicted_tw = route.eval_insert_time_warp(pos, 2);

        Route copy(data, 0);
        copy.set_clients({0, 1});
        copy.insert(pos, 2);

        CHECK(predicted_tw == copy.time_warp());
    }
}

TEST_CASE("Route: eval_remove_time_warp matches actual remove",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});

    for (int pos = 0; pos < route.size(); ++pos) {
        int predicted_tw = route.eval_remove_time_warp(pos);

        Route copy(data, 0);
        copy.set_clients({0, 1, 2});
        copy.remove(pos);

        CHECK(predicted_tw == copy.time_warp());
    }
}

TEST_CASE("Route: eval_insert_time_warp on infeasible instance",
          "[route][duration]")
{
    auto data = make_infeasible_tw_instance();
    Route route(data, 0);

    route.set_clients({0});

    // Insert client 1 after client 0.
    int predicted_tw = route.eval_insert_time_warp(1, 1);

    Route verify(data, 0);
    verify.set_clients({0, 1});

    CHECK(predicted_tw == verify.time_warp());
}

TEST_CASE("Route: eval_remove_time_warp reduces time warp",
          "[route][duration]")
{
    auto data = make_infeasible_tw_instance();
    Route route(data, 0);

    route.set_clients({0, 1});
    CHECK(route.time_warp() > 0);

    // Removing client 1 should eliminate the time warp.
    int predicted_tw = route.eval_remove_time_warp(1);

    Route verify(data, 0);
    verify.set_clients({0});

    CHECK(predicted_tw == verify.time_warp());
    CHECK(predicted_tw == 0);
}

TEST_CASE("Route: eval_insert_time_warp into empty route",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    int tw = route.eval_insert_time_warp(0, 0);
    CHECK(tw == 0);  // Single feasible client from depot.
}

TEST_CASE("Route: eval_insert_time_warp with tight TW client into empty route",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    // Client 3 at (0, 10), TW [0, 15], service = 3.
    // depot -> c3: travel = 10. Arrive at 10. TW [0, 15], 10 <= 15.
    // Depart at max(10, 0) + 3 = 13.
    // c3 -> depot: travel = 10. Arrive at depot at 23.
    // Depot TW [0, 1000]: 23 <= 1000, feasible.
    int tw = route.eval_insert_time_warp(0, 3);
    CHECK(tw == 0);
}

TEST_CASE("Route: time warp with waiting scenario",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    // Visit client 2 (TW [50, 100]) then client 0 (TW [5, 50]).
    // depot -> c2: travel = 30. Arrive 30, wait until 50, depart 55.
    // c2 -> c0: travel = 20. Arrive at 75. TW [5, 50], 75 > 50 => warp 25.
    route.set_clients({2, 0});

    CHECK(route.time_warp() == 25);
    CHECK_FALSE(route.tw_feasible());
}

// ===========================================================================
//  DurationResource latest (backward propagation) tests
// ===========================================================================

TEST_CASE("DurationResource::merge latest propagation",
          "[duration_resource]")
{
    auto data = make_vrptw_instance();

    auto depot = DurationResource::init_depot(data, 0);
    auto c0 = DurationResource::init(data, 0, 1);  // TW [5, 50], svc=5

    // depot -> c0: travel = 10.
    auto d_c0 = DurationResource::merge(depot, c0, data, 0);

    // Latest for the combined: min(L_depot, L_c0 - travel - D_depot + W_depot)
    // = min(1000, 50 - 10 - 0 + 0) = min(1000, 40) = 40.
    CHECK(d_c0.latest == 40);
}

// ===========================================================================
//  Suffix array tests
// ===========================================================================

TEST_CASE("Route: dur_prefix and dur_suffix are consistent",
          "[route][duration]")
{
    auto data = make_vrptw_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});

    // The full route time warp should match whether we compute it from
    // prefix or by merging prefix + suffix.
    int profile = data.vehicle_type(0).profile;
    auto const& prefix_full = route.dur_prefix(2);
    auto depot_end = DurationResource::init_depot(data, 0);
    auto full = DurationResource::merge(prefix_full, depot_end, data, profile);

    CHECK(DurationResource::time_warp(full) == route.time_warp());
}
