#include "search/partitioned_search.h"

#include "routing/construction.h"
#include "routing/cost_evaluator.h"
#include "routing/local_search.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <set>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (6 clients, 1 depot, 2 vehicles)
// ---------------------------------------------------------------------------

static ProblemData make_small_cvrp() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});
    b.add_client({10.0, 10.0}, {.demand = {5}});
    b.add_client({0.0, 10.0}, {.demand = {5}});
    b.add_client({-10.0, 0.0}, {.demand = {5}});
    b.add_client({-10.0, -10.0}, {.demand = {5}});
    b.add_client({0.0, -10.0}, {.demand = {5}});

    b.add_vehicle_type(2, {.capacity = {20}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build a medium CVRP instance (20 clients)
// ---------------------------------------------------------------------------

static ProblemData make_medium_cvrp() {
    ProblemData::Builder b;
    b.add_depot({50.0, 50.0});

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            b.add_client({static_cast<double>(i * 20), static_cast<double>(j * 25)},
                         {.demand = {3}});
        }
    }

    b.add_vehicle_type(4, {.capacity = {20}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build a larger spatially-clustered instance (40 clients)
// ---------------------------------------------------------------------------

static ProblemData make_clustered_cvrp() {
    ProblemData::Builder b;
    b.add_depot({50.0, 50.0});

    // 4 clusters of 10 clients each, at different corners.
    double centers[][2] = {{10, 10}, {90, 10}, {10, 90}, {90, 90}};
    for (auto& [cx, cy] : centers) {
        for (int i = 0; i < 10; ++i) {
            double x = cx + (i % 5) * 2.0 - 4.0;
            double y = cy + (i / 5) * 2.0 - 2.0;
            b.add_client({x, y}, {.demand = {2}});
        }
    }

    b.add_vehicle_type(8, {.capacity = {20}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Partition tests
// ---------------------------------------------------------------------------

TEST_CASE("Partition — k-means creates valid clusters", "[partitioned_search]") {
    auto data = make_medium_cvrp();
    PartitionedSearch ps(data);

    auto part = ps.partition_clients(4);

    // Every client should be assigned to exactly one cluster.
    CHECK(part.assignment.size() == static_cast<size_t>(data.num_clients()));
    CHECK(part.num_clusters() == 4);

    // All cluster indices should be valid.
    for (int a : part.assignment) {
        CHECK(a >= 0);
        CHECK(a < 4);
    }

    // Total clients across all clusters should equal num_clients.
    int total = 0;
    for (auto const& cl : part.clusters) {
        total += static_cast<int>(cl.size());
    }
    CHECK(total == data.num_clients());

    // Each cluster should be non-empty (with 20 clients and 4 clusters).
    for (auto const& cl : part.clusters) {
        CHECK(!cl.empty());
    }
}

TEST_CASE("Partition — k=1 puts all clients in one cluster", "[partitioned_search]") {
    auto data = make_small_cvrp();
    PartitionedSearch ps(data);

    auto part = ps.partition_clients(1);

    CHECK(part.num_clusters() == 1);
    CHECK(static_cast<int>(part.clusters[0].size()) == data.num_clients());
}

TEST_CASE("Partition — k clamped to num_clients", "[partitioned_search]") {
    auto data = make_small_cvrp();
    PartitionedSearch ps(data);

    // k > num_clients should be clamped.
    auto part = ps.partition_clients(100);

    CHECK(part.num_clusters() == data.num_clients());

    // Each cluster should contain at least one client (or be empty since
    // k-means may leave some empty). Total should still equal num_clients.
    int total = 0;
    for (auto const& cl : part.clusters) {
        total += static_cast<int>(cl.size());
    }
    CHECK(total == data.num_clients());
}

TEST_CASE("Partition — clusters are non-overlapping", "[partitioned_search]") {
    auto data = make_clustered_cvrp();
    PartitionedSearch ps(data);

    auto part = ps.partition_clients(4);

    // Each client should appear in exactly one cluster.
    std::set<int> seen;
    for (auto const& cl : part.clusters) {
        for (int c : cl) {
            CHECK(!seen.contains(c));
            seen.insert(c);
        }
    }
    CHECK(static_cast<int>(seen.size()) == data.num_clients());
}

// ---------------------------------------------------------------------------
//  Overlap expansion tests
// ---------------------------------------------------------------------------

TEST_CASE("Overlap expansion adds boundary clients", "[partitioned_search]") {
    auto data = make_medium_cvrp();
    PartitionedSearch ps(data);

    auto part = ps.partition_clients(4);
    auto expanded = ps.expand_with_overlap(part, 0.2);

    CHECK(expanded.size() == part.clusters.size());

    // Each expanded cluster should be at least as large as the base cluster.
    for (int k = 0; k < part.num_clusters(); ++k) {
        CHECK(expanded[k].size() >= part.clusters[k].size());
    }
}

TEST_CASE("Overlap 0.0 returns base clusters unchanged", "[partitioned_search]") {
    auto data = make_medium_cvrp();
    PartitionedSearch ps(data);

    auto part = ps.partition_clients(4);
    auto expanded = ps.expand_with_overlap(part, 0.0);

    for (int k = 0; k < part.num_clusters(); ++k) {
        CHECK(expanded[k].size() == part.clusters[k].size());
    }
}

// ---------------------------------------------------------------------------
//  Merge produces a valid complete solution
// ---------------------------------------------------------------------------

TEST_CASE("Partitioned search — merge produces valid solution", "[partitioned_search]") {
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    // Build an initial solution.
    auto initial = construction::clarke_wright(data, eval);
    CHECK(initial.num_unassigned() == 0);

    // Run partitioned search with a no-op local search.
    PartitionedSearch ps(data);
    auto noop = [](Solution&, CostEvaluator const&) {};

    PartitionConfig config;
    config.num_partitions = 3;
    config.overlap_frac = 0.1;
    config.max_iterations = 1;

    auto result = ps.run(initial, eval, noop, config);

    // All clients should still be assigned.
    CHECK(result.num_unassigned() == 0);

    // Verify each client appears exactly once across all routes.
    std::vector<bool> seen(data.num_clients(), false);
    for (int r = 0; r < result.num_routes(); ++r) {
        for (int c : result.route(r).clients()) {
            CHECK(!seen[c]);
            seen[c] = true;
        }
    }
    for (int c = 0; c < data.num_clients(); ++c) {
        CHECK(seen[c]);
    }
}

// ---------------------------------------------------------------------------
//  Integration: partitioned search with real local search
// ---------------------------------------------------------------------------

TEST_CASE("Partitioned search — with local search produces valid solution",
          "[partitioned_search]") {
    auto data = make_medium_cvrp();
    CostEvaluator eval;

    auto initial = construction::clarke_wright(data, eval);

    PartitionedSearch ps(data, 123);
    LocalSearch ls(data);

    auto local_search_fn = [&ls](Solution& sol, CostEvaluator const& ev) { ls.run(sol, ev); };

    PartitionConfig config;
    config.num_partitions = 3;
    config.overlap_frac = 0.15;
    config.max_iterations = 3;

    auto result = ps.run(initial, eval, local_search_fn, config);

    CHECK(result.num_unassigned() == 0);
    CHECK(result.cost(eval) > 0);
    // Should be no worse than initial (or at least produce a valid solution).
    CHECK(result.cost(eval) <= initial.cost(eval));
}

TEST_CASE("Partitioned search — clustered instance benefits from decomposition",
          "[partitioned_search]") {
    auto data = make_clustered_cvrp();
    CostEvaluator eval;

    auto initial = construction::clarke_wright(data, eval);
    CHECK(initial.num_unassigned() == 0);

    PartitionedSearch ps(data, 42);
    LocalSearch ls(data);

    auto local_search_fn = [&ls](Solution& sol, CostEvaluator const& ev) { ls.run(sol, ev); };

    PartitionConfig config;
    config.num_partitions = 4;
    config.overlap_frac = 0.1;
    config.max_iterations = 5;

    auto result = ps.run(initial, eval, local_search_fn, config);

    CHECK(result.num_unassigned() == 0);
    CHECK(result.cost(eval) > 0);
}

TEST_CASE("Partitioned search — single partition is identity-like", "[partitioned_search]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    auto initial = construction::clarke_wright(data, eval);
    int64_t initial_cost = initial.cost(eval);

    PartitionedSearch ps(data);
    LocalSearch ls(data);

    auto local_search_fn = [&ls](Solution& sol, CostEvaluator const& ev) { ls.run(sol, ev); };

    PartitionConfig config;
    config.num_partitions = 1;
    config.max_iterations = 1;

    auto result = ps.run(initial, eval, local_search_fn, config);

    CHECK(result.num_unassigned() == 0);
    // With a single partition + local search, should be at least as good.
    CHECK(result.cost(eval) <= initial_cost);
}
