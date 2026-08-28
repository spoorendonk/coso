#include "model/instance_reader.h"
#include "model/routing_model.h"
#include "model/types.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Helper: locate the tests/data/ directory relative to the executable or
//  via the COSO_DATA_DIR environment variable.
// ---------------------------------------------------------------------------

static std::string data_dir() {
    if (auto const* env = std::getenv("COSO_DATA_DIR"); env && *env) {
        return env;
    }

    // Walk up from the build directory to find tests/data/.
    // CMake typically puts test executables in build/ or build/tests/.
    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto candidate = p / "tests" / "data";
        if (fs::is_directory(candidate)) {
            return candidate.string();
        }
    }

    // Fallback: relative path (works when run from repo root).
    return "tests/data";
}

static std::string instance_path(const std::string& filename) {
    return data_dir() + "/" + filename;
}

static bool instance_exists(const std::string& filename) {
    return fs::is_regular_file(instance_path(filename));
}

// ---------------------------------------------------------------------------
//  Benchmark tests -- tagged [benchmark] so they can be run selectively.
//  Each test SKIPs when the benchmark instance file is not present.
// ---------------------------------------------------------------------------

TEST_CASE("X-n101-k25 end-to-end benchmark", "[benchmark][routing]") {
    const std::string file = "X-n101-k25.vrp";

    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file +
             " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    INFO("Instance: " << instance_path(file));

    // Read instance to verify it parses correctly.
    auto inst = coso::read_vrp(instance_path(file));
    REQUIRE(inst.dimension == 101);
    REQUIRE(inst.capacity > 0);

    // Solve with a short time limit.
    auto result = coso::solve(instance_path(file), coso::TimeLimit(10));

    std::cout << "\n=== X-n101-k25 benchmark ===\n"
              << "  Cost:       " << result.cost() << "\n"
              << "  Feasible:   " << (result.feasible() ? "yes" : "no") << "\n"
              << "  Routes:     " << result.routes().size() << "\n"
              << "  Unserved:   " << result.unserved().size() << "\n"
              << "  Elapsed:    " << result.elapsed_seconds() << "s\n"
              << "  Iterations: " << result.iterations() << "\n"
              << "  BKS:        27591\n"
              << std::endl;

    // The solution must be feasible.
    CHECK(result.feasible());

    // All clients should be served.
    CHECK(result.unserved().empty());

    // Must have at least one route.
    CHECK(!result.routes().empty());

    // Cost should be within a reasonable range of the BKS (27591).
    // Allow up to 15% gap for a 10-second run.
    constexpr double bks = 27591.0;
    constexpr double max_gap = 0.15;
    if (result.feasible() && result.cost() > 0) {
        double gap = (result.cost() - bks) / bks;
        std::cout << "  Gap to BKS: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(result.cost() <= bks * (1.0 + max_gap));
    }
}

TEST_CASE("X-n106-k14 end-to-end benchmark", "[benchmark][routing]") {
    const std::string file = "X-n106-k14.vrp";

    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file +
             " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    INFO("Instance: " << instance_path(file));

    auto inst = coso::read_vrp(instance_path(file));
    REQUIRE(inst.dimension == 106);

    auto result = coso::solve(instance_path(file), coso::TimeLimit(10));

    std::cout << "\n=== X-n106-k14 benchmark ===\n"
              << "  Cost:       " << result.cost() << "\n"
              << "  Feasible:   " << (result.feasible() ? "yes" : "no") << "\n"
              << "  Routes:     " << result.routes().size() << "\n"
              << "  Elapsed:    " << result.elapsed_seconds() << "s\n"
              << "  BKS:        26362\n"
              << std::endl;

    CHECK(result.feasible());
    CHECK(result.unserved().empty());
    CHECK(!result.routes().empty());

    // BKS for X-n106-k14 is 26362.
    constexpr double bks = 26362.0;
    constexpr double max_gap = 0.15;
    if (result.feasible() && result.cost() > 0) {
        double gap = (result.cost() - bks) / bks;
        std::cout << "  Gap to BKS: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(result.cost() <= bks * (1.0 + max_gap));
    }
}

TEST_CASE("X-n110-k13 end-to-end benchmark", "[benchmark][routing]") {
    const std::string file = "X-n110-k13.vrp";

    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file +
             " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    INFO("Instance: " << instance_path(file));

    auto inst = coso::read_vrp(instance_path(file));
    REQUIRE(inst.dimension == 110);

    auto result = coso::solve(instance_path(file), coso::TimeLimit(10));

    std::cout << "\n=== X-n110-k13 benchmark ===\n"
              << "  Cost:       " << result.cost() << "\n"
              << "  Feasible:   " << (result.feasible() ? "yes" : "no") << "\n"
              << "  Routes:     " << result.routes().size() << "\n"
              << "  Elapsed:    " << result.elapsed_seconds() << "s\n"
              << "  BKS:        14971\n"
              << std::endl;

    CHECK(result.feasible());
    CHECK(result.unserved().empty());
    CHECK(!result.routes().empty());

    // BKS for X-n110-k13 is 14971.
    constexpr double bks = 14971.0;
    constexpr double max_gap = 0.15;
    if (result.feasible() && result.cost() > 0) {
        double gap = (result.cost() - bks) / bks;
        std::cout << "  Gap to BKS: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(result.cost() <= bks * (1.0 + max_gap));
    }
}
