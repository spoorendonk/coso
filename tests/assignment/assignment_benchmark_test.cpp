#include "assignment/assignment_solution.h"
#include "assignment/construction.h"
#include "assignment/cost_evaluator.h"
#include "assignment/operators/block_swap.h"
#include "assignment/operators/shift_move.h"
#include "assignment/operators/shift_swap.h"
#include "assignment/parsers.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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
    return data_dir() + "/nrp/" + filename;
}

static bool instance_exists(const std::string& filename) {
    return fs::is_regular_file(instance_path(filename));
}

// ---------------------------------------------------------------------------
//  Helper: run local search on an assignment solution.
// ---------------------------------------------------------------------------

static void run_local_search(coso::AssignmentSolution& sol, int max_iters = 200) {
    coso::ShiftMove move_op;
    coso::ShiftSwap swap_op;
    coso::BlockSwap block_op;

    for (int iter = 0; iter < max_iters; ++iter) {
        bool improved = false;

        if (move_op.find_best_move(sol)) {
            move_op.apply(sol);
            improved = true;
            continue;
        }

        if (swap_op.find_best_move(sol)) {
            swap_op.apply(sol);
            improved = true;
            continue;
        }

        if (block_op.find_best_move(sol)) {
            block_op.apply(sol);
            improved = true;
            continue;
        }

        if (!improved) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
//  Helper: run a single NRP benchmark test.
// ---------------------------------------------------------------------------

static void run_nrp_benchmark(const std::string& file, int expected_employees,
                              int expected_horizon_days, int bks, double max_gap = 1.0) {
    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file +
             " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    INFO("Instance: " << instance_path(file));

    auto data = coso::read_nrp(instance_path(file));
    CHECK(data.num_employees() == expected_employees);
    CHECK(data.horizon == expected_horizon_days);
    REQUIRE(data.num_shift_types() > 0);

    // NRP COVER lines encode under/over staffing penalties.
    // The benchmark instances in tests/data/nrp use under=100, over=1.
    coso::AssignmentCostEvaluator::Weights weights;
    weights.understaffing = 100;
    weights.overstaffing = 1;
    coso::AssignmentCostEvaluator evaluator(data, weights);

    // Construct initial solution using FFD.
    auto sol = coso::construct_ffd(data, evaluator);

    int initial_cost = sol.cost();

    // Run local search to improve.
    run_local_search(sol);

    int final_cost = sol.cost();

    std::cout << "\n=== " << file << " NRP benchmark ===\n"
              << "  Employees:     " << data.num_employees() << "\n"
              << "  Horizon:       " << data.horizon << " days\n"
              << "  Shift types:   " << data.num_shift_types() << "\n"
              << "  Initial cost:  " << initial_cost << "\n"
              << "  Final cost:    " << final_cost << "\n"
              << "  Feasible:      " << (sol.is_feasible() ? "yes" : "no") << "\n"
              << "  Demand cost:   " << sol.demand_cost() << "\n"
              << "  Hard cost:     " << sol.hard_constraint_cost() << "\n"
              << "  Pref cost:     " << sol.preference_cost() << "\n"
              << "  BKS:           " << bks << "\n"
              << std::endl;

    // Local search should improve or maintain cost.
    CHECK(final_cost <= initial_cost);

    // Cost should be within a reasonable range of the BKS.
    // NRP BKS values are soft-cost only; our cost includes demand penalties,
    // so we use a generous gap.
    if (bks > 0 && final_cost > 0) {
        double gap = static_cast<double>(final_cost - bks) / bks;
        std::cout << "  Gap to BKS: " << (gap * 100.0) << "%\n" << std::endl;
        CHECK(final_cost <= static_cast<int>(bks * (1.0 + max_gap)));
    }
}

// ---------------------------------------------------------------------------
//  Small instances (2-week horizon, fast to solve).
// ---------------------------------------------------------------------------

TEST_CASE("NRP Instance1 benchmark", "[benchmark][nrp][assignment]") {
    // Instance1: 8 employees, 14 days (2 weeks), 1 shift type, BKS = 607
    run_nrp_benchmark("Instance1.txt", 8, 14, 607);
}

TEST_CASE("NRP Instance2 benchmark", "[benchmark][nrp][assignment]") {
    // Instance2: 14 employees, 14 days (2 weeks), 2 shift types, BKS = 828
    run_nrp_benchmark("Instance2.txt", 14, 14, 828);
}

TEST_CASE("NRP Instance3 benchmark", "[benchmark][nrp][assignment]") {
    // Instance3: 20 employees, 14 days (2 weeks), 3 shift types, BKS = 1001
    run_nrp_benchmark("Instance3.txt", 20, 14, 1001);
}

// ---------------------------------------------------------------------------
//  Medium instances (4-week horizon).
// ---------------------------------------------------------------------------

TEST_CASE("NRP Instance4 benchmark", "[benchmark][nrp][assignment]") {
    // Instance4: 10 employees, 28 days (4 weeks), 2 shift types, BKS = 1716
    run_nrp_benchmark("Instance4.txt", 10, 28, 1716);
}

TEST_CASE("NRP Instance5 benchmark", "[benchmark][nrp][assignment]") {
    // Instance5: 16 employees, 28 days (4 weeks), 2 shift types, BKS = 1143
    run_nrp_benchmark("Instance5.txt", 16, 28, 1143);
}

TEST_CASE("NRP Instance6 benchmark", "[benchmark][nrp][assignment]") {
    // Instance6: 18 employees, 28 days (4 weeks), 3 shift types, BKS = 1950
    run_nrp_benchmark("Instance6.txt", 18, 28, 1950);
}

TEST_CASE("NRP Instance7 benchmark", "[benchmark][nrp][assignment]") {
    // Instance7: 20 employees, 28 days (4 weeks), 3 shift types, BKS = 1056
    run_nrp_benchmark("Instance7.txt", 20, 28, 1056);
}

TEST_CASE("NRP Instance8 benchmark", "[benchmark][nrp][assignment]") {
    // Instance8: 30 employees, 28 days (4 weeks), 4 shift types, BKS = 1300
    run_nrp_benchmark("Instance8.txt", 30, 28, 1300);
}
