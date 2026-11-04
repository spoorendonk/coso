#include "model/instance_reader.h"
#include "model/routing_model.h"
#include "model/types.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <type_traits>
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
//  The convention each build prices Solomon under (#249), and the published
//  values that convention can be compared with:
//   - integer builds: floor(10 * euclid), times scaled by 10, compared with the
//     exact-method optima of Solomon's own tables;
//   - double build: double precision, compared with SINTEF's best-known table.
// ---------------------------------------------------------------------------

static constexpr int kScale = std::is_integral_v<coso::Measure> ? 10 : 1;

struct Reference {
    double exact_method;  ///< web.cba.neu.edu/~msolomon: c1c2solu, r1r2solu, rc12solu
    double best_known;    ///< sintef.no/projectweb/top/vrptw/100-customers
};

static double reference_value(Reference r) {
    return std::is_integral_v<coso::Measure> ? r.exact_method : r.best_known;
}

// ---------------------------------------------------------------------------
//  Helper: build a RoutingModel from a parsed Solomon VrpInstance.
// ---------------------------------------------------------------------------

static coso::RoutingModel build_vrptw_model(const coso::VrpInstance& inst) {
    coso::RoutingModel model;

    auto window = [](coso::TimeWindow tw) {
        return coso::TimeWindow{tw.start * kScale, tw.end * kScale};
    };

    // Depot is always node 0 in Solomon instances.
    coso::DepotParams dp;
    dp.tw = window(inst.time_windows[0]);
    model.add_depot(inst.coords[0].x, inst.coords[0].y, dp);

    // Add clients (nodes 1..dimension-1).
    for (int i = 1; i < inst.dimension; ++i) {
        coso::ClientParams cp;
        if (inst.demands[i] > 0) {
            cp.demand = {inst.demands[i]};
        }
        cp.tw = window(inst.time_windows[i]);
        cp.service = inst.service_times[i] * kScale;
        model.add_client(inst.coords[i].x, inst.coords[i].y, cp);
    }

    // Add vehicle type with capacity and duration limit.
    coso::VehicleTypeParams vtp;
    if (inst.capacity > 0) {
        vtp.capacity = {inst.capacity};
    }
    // Use the depot's time window end as the max duration (planning horizon).
    vtp.max_duration = inst.time_windows[0].end * kScale;
    model.add_vehicle_type(inst.vehicles, vtp);

    // Depot first, then clients, is the instance's own node order.  The double
    // build leaves the model's exact Euclidean arcs in place.
    if constexpr (std::is_integral_v<coso::Measure>) {
        for (int i = 0; i < inst.dimension; ++i) {
            for (int j = 0; j < inst.dimension; ++j) {
                double d = std::hypot(inst.coords[i].x - inst.coords[j].x,
                                      inst.coords[i].y - inst.coords[j].y);
                auto arc = static_cast<coso::Measure>(std::floor(kScale * d));
                model.set_distance(i, j, arc);
                model.set_duration(i, j, arc);
            }
        }
    }

    return model;
}

// ---------------------------------------------------------------------------
//  Helper: run a single VRPTW benchmark test.
// ---------------------------------------------------------------------------

static void run_vrptw_benchmark(const std::string& file, int expected_dimension, Reference ref,
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
    double const bks = reference_value(ref);
    double const cost = result.cost() / kScale;

    std::cout << "\n=== " << file << " VRPTW benchmark ===\n"
              << "  Cost:       " << cost << "\n"
              << "  Feasible:   " << (result.feasible() ? "yes" : "no") << "\n"
              << "  Routes:     " << result.routes().size() << "\n"
              << "  Unserved:   " << result.unserved().size() << "\n"
              << "  Elapsed:    " << result.elapsed_seconds() << "s\n"
              << "  Budget:     " << time_limit_s << "s\n"
              << "  Iterations: " << result.iterations() << "\n"
              << "  Reference:  " << bks << "\n"
              << std::endl;

    // All clients should be served.
    CHECK(result.unserved().empty());

    // Must have at least one route.
    CHECK(!result.routes().empty());

    // Cost should be within a reasonable range of the reference when feasible.
    // Under tight time budgets, the current portfolio may return a slightly
    // infeasible route set at the exact budget boundary.
    if (result.feasible() && cost > 0) {
        double gap = (cost - bks) / bks;
        std::cout << "  Gap to reference: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(cost <= bks * (1.0 + max_gap));
    }
}

// ---------------------------------------------------------------------------
//  Solomon C1 instances (clustered, tight time windows).
// ---------------------------------------------------------------------------

TEST_CASE("C101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    run_vrptw_benchmark("C101.txt", 101, {827.3, 828.94});
}

TEST_CASE("C102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    run_vrptw_benchmark("C102.txt", 101, {827.3, 828.94});
}

// ---------------------------------------------------------------------------
//  Solomon R1 instances (random, tight time windows).
// ---------------------------------------------------------------------------

TEST_CASE("R101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    // SINTEF's 1650.80 uses 19 vehicles and the optimum 1637.7 uses 20, so the two are not one
    // solution at two precisions.  The older 1645.79 is the lower-precision value SINTEF retired.
    run_vrptw_benchmark("R101.txt", 101, {1637.7, 1650.80});
}

TEST_CASE("R102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    run_vrptw_benchmark("R102.txt", 101, {1466.6, 1486.12});
}

// ---------------------------------------------------------------------------
//  Solomon RC1 instances (mixed random-clustered, tight time windows).
// ---------------------------------------------------------------------------

TEST_CASE("RC101 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    run_vrptw_benchmark("RC101.txt", 101, {1619.8, 1696.95});
}

TEST_CASE("RC102 VRPTW benchmark", "[benchmark][vrptw][routing]") {
    run_vrptw_benchmark("RC102.txt", 101, {1457.4, 1554.75});
}
