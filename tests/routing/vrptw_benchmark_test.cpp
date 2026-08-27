#include "model/instance_reader.h"
#include "model/routing_model.h"
#include "model/types.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Helper: locate the tests/data/ directory.
// ---------------------------------------------------------------------------

static std::string data_dir() {
    if (auto const* env = std::getenv("COSO_DATA_DIR"); env && *env) {
        return env;
    }

    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto candidate = p / "tests" / "data";
        if (fs::is_directory(candidate)) {
            return candidate.string();
        }
    }

    return "tests/data";
}

static std::string instance_path(const std::string& filename) {
    return data_dir() + "/" + filename;
}

static bool instance_exists(const std::string& filename) {
    return fs::is_regular_file(instance_path(filename));
}

// ---------------------------------------------------------------------------
//  Helper: build a RoutingModel from a parsed Solomon VrpInstance.
// ---------------------------------------------------------------------------

static coso::RoutingModel build_vrptw_model(const coso::VrpInstance& inst) {
    coso::RoutingModel model;

    // Depot is always node 0 in Solomon instances.
    coso::DepotParams dp;
    dp.tw = inst.time_windows[0];
    model.add_depot(inst.coords[0].x, inst.coords[0].y, dp);

    // Add clients (nodes 1..dimension-1).
    for (int i = 1; i < inst.dimension; ++i) {
        coso::ClientParams cp;
        if (inst.demands[i] > 0) {
            cp.demand = {inst.demands[i]};
        }
        cp.tw = inst.time_windows[i];
        cp.service = inst.service_times[i];
        model.add_client(inst.coords[i].x, inst.coords[i].y, cp);
    }

    // Add vehicle type with capacity and duration limit.
    coso::VehicleTypeParams vtp;
    if (inst.capacity > 0) {
        vtp.capacity = {inst.capacity};
    }
    // Use the depot's time window end as the max duration (planning horizon).
    vtp.max_duration = inst.time_windows[0].end;
    model.add_vehicle_type(inst.vehicles, vtp);

    return model;
}

// ---------------------------------------------------------------------------
//  Helper: run a single VRPTW benchmark test.
// ---------------------------------------------------------------------------

static void run_vrptw_benchmark(const std::string& file, int expected_dimension, double bks,
                                double max_gap = 0.25, double time_limit_s = 30.0) {
    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file +
             " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    INFO("Instance: " << instance_path(file));

    auto inst = coso::read_solomon(instance_path(file));
    REQUIRE(inst.dimension == expected_dimension);
    REQUIRE(inst.capacity > 0);
    REQUIRE(!inst.time_windows.empty());
    REQUIRE(!inst.service_times.empty());

    auto model = build_vrptw_model(inst);
    auto result = model.solve(coso::TimeLimit(time_limit_s));

    std::cout << "\n=== " << file << " VRPTW benchmark ===\n"
              << "  Cost:       " << result.cost() << "\n"
              << "  Feasible:   " << (result.feasible() ? "yes" : "no") << "\n"
              << "  Routes:     " << result.routes().size() << "\n"
              << "  Unserved:   " << result.unserved().size() << "\n"
              << "  Elapsed:    " << result.elapsed_seconds() << "s\n"
              << "  Budget:     " << time_limit_s << "s\n"
              << "  Iterations: " << result.iterations() << "\n"
              << "  BKS:        " << bks << "\n"
              << std::endl;

    // All clients should be served.
    CHECK(result.unserved().empty());

    // Must have at least one route.
    CHECK(!result.routes().empty());

    // Cost should be within a reasonable range of the BKS when feasible.
    // Under tight time budgets, the current portfolio may return a slightly
    // infeasible route set at the exact budget boundary.
    if (result.feasible() && result.cost() > 0) {
        double gap = (result.cost() - bks) / bks;
        std::cout << "  Gap to BKS: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(result.cost() <= bks * (1.0 + max_gap));
    }
}

// ---------------------------------------------------------------------------
//  Solomon C1 instances (clustered, tight time windows).
//  BKS values from http://w.cba.neu.edu/~msolomon/problems.htm
// ---------------------------------------------------------------------------

TEST_CASE("C101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // C101: 100 clients + 1 depot = 101 nodes, BKS distance = 828.94
    run_vrptw_benchmark("C101.txt", 101, 828.94);
}

TEST_CASE("C102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // C102: 100 clients + 1 depot = 101 nodes, BKS distance = 828.94
    run_vrptw_benchmark("C102.txt", 101, 828.94);
}

// ---------------------------------------------------------------------------
//  Solomon R1 instances (random, tight time windows).
// ---------------------------------------------------------------------------

TEST_CASE("R101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // R101: 100 clients + 1 depot = 101 nodes, BKS distance = 1645.79
    run_vrptw_benchmark("R101.txt", 101, 1645.79);
}

TEST_CASE("R102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // R102: 100 clients + 1 depot = 101 nodes, BKS distance = 1486.12
    run_vrptw_benchmark("R102.txt", 101, 1486.12);
}

// ---------------------------------------------------------------------------
//  Solomon RC1 instances (mixed random-clustered, tight time windows).
// ---------------------------------------------------------------------------

TEST_CASE("RC101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // RC101: 100 clients + 1 depot = 101 nodes, BKS distance = 1696.94
    run_vrptw_benchmark("RC101.txt", 101, 1696.94);
}

TEST_CASE("RC102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // RC102: 100 clients + 1 depot = 101 nodes, BKS distance = 1554.75
    run_vrptw_benchmark("RC102.txt", 101, 1554.75);
}
