#include <catch2/catch_test_macros.hpp>

#include "search/benchmarker.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Helper: locate the tests/data/ directory.
// ---------------------------------------------------------------------------

static std::string data_dir()
{
    if (auto const* env = std::getenv("COSO_DATA_DIR"); env && *env) {
        return env;
    }

    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto candidate = p / "tests" / "data";
        if (fs::is_directory(candidate))
            return candidate.string();
    }

    return "tests/data";
}

static std::string instance_path(const std::string& filename)
{
    return data_dir() + "/" + filename;
}

static bool instance_exists(const std::string& filename)
{
    return fs::is_regular_file(instance_path(filename));
}

// ---------------------------------------------------------------------------
//  BKS lookup tests (always run, no instances needed)
// ---------------------------------------------------------------------------

TEST_CASE("BKS lookup returns known values", "[benchmarker]")
{
    CHECK(coso::Benchmarker::lookup_bks("X-n101-k25.vrp") == 27591);
    CHECK(coso::Benchmarker::lookup_bks("X-n106-k14.vrp") == 26362);
    CHECK(coso::Benchmarker::lookup_bks("X-n110-k13.vrp") == 14971);
    CHECK(coso::Benchmarker::lookup_bks("X-n120-k6.vrp") == 6942);
    CHECK(coso::Benchmarker::lookup_bks("X-n125-k30.vrp") == 55539);
}

TEST_CASE("BKS lookup returns 0 for unknown instances", "[benchmarker]")
{
    CHECK(coso::Benchmarker::lookup_bks("unknown.vrp") == 0);
    CHECK(coso::Benchmarker::lookup_bks("") == 0);
}

// ---------------------------------------------------------------------------
//  CSV formatting tests (no instances needed)
// ---------------------------------------------------------------------------

TEST_CASE("to_csv produces correct header and rows", "[benchmarker]")
{
    std::vector<coso::BenchmarkResult> results = {
        {.instance = "test1.vrp", .bks = 1000, .cost = 1050,
         .gap_pct = 5.0, .elapsed_s = 1.234, .feasible = true,
         .num_routes = 3},
        {.instance = "test2.vrp", .bks = 2000, .cost = 2200,
         .gap_pct = 10.0, .elapsed_s = 2.567, .feasible = false,
         .num_routes = 5},
    };

    auto csv = coso::Benchmarker::to_csv(results);

    // Check header.
    CHECK(csv.find("instance,bks,cost,gap_pct,elapsed_s,feasible,num_routes\n")
          == 0);

    // Check data rows are present.
    CHECK(csv.find("test1.vrp,1000,1050,5.00,1.234,true,3\n")
          != std::string::npos);
    CHECK(csv.find("test2.vrp,2000,2200,10.00,2.567,false,5\n")
          != std::string::npos);
}

TEST_CASE("to_csv handles empty results", "[benchmarker]")
{
    std::vector<coso::BenchmarkResult> results;
    auto csv = coso::Benchmarker::to_csv(results);
    CHECK(csv == "instance,bks,cost,gap_pct,elapsed_s,feasible,num_routes\n");
}

// ---------------------------------------------------------------------------
//  print_summary smoke test (no instances needed)
// ---------------------------------------------------------------------------

TEST_CASE("print_summary does not crash on empty results", "[benchmarker]")
{
    std::vector<coso::BenchmarkResult> results;
    coso::Benchmarker::print_summary(results);
}

TEST_CASE("print_summary does not crash with results", "[benchmarker]")
{
    std::vector<coso::BenchmarkResult> results = {
        {.instance = "A.vrp", .bks = 100, .cost = 110,
         .gap_pct = 10.0, .elapsed_s = 1.0, .feasible = true,
         .num_routes = 2},
        {.instance = "B.vrp", .bks = 200, .cost = 220,
         .gap_pct = 10.0, .elapsed_s = 2.0, .feasible = true,
         .num_routes = 3},
    };
    coso::Benchmarker::print_summary(results);
}

// ---------------------------------------------------------------------------
//  Integration: run_instance (requires downloaded benchmarks)
// ---------------------------------------------------------------------------

TEST_CASE("run_instance solves X-n101-k25", "[benchmarker][benchmark]")
{
    const std::string file = "X-n101-k25.vrp";

    if (!instance_exists(file)) {
        SKIP("Benchmark instance " + file + " not found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    coso::Benchmarker bench(5.0);
    auto result = bench.run_instance(instance_path(file), 27591);

    CHECK(result.instance == file);
    CHECK(result.bks == 27591);
    CHECK(result.feasible);
    CHECK(result.num_routes > 0);
    CHECK(result.elapsed_s > 0.0);
    CHECK(result.cost > 0);
    // Allow up to 15% gap for a 5-second run.
    CHECK(result.gap_pct < 15.0);
}

// ---------------------------------------------------------------------------
//  Integration: run_directory (requires downloaded benchmarks)
// ---------------------------------------------------------------------------

TEST_CASE("run_directory finds and runs .vrp instances", "[benchmarker][benchmark]")
{
    auto dir = data_dir();

    if (!instance_exists("X-n101-k25.vrp")) {
        SKIP("No benchmark instances found. "
             "Run tests/data/download_benchmarks.sh first.");
    }

    coso::Benchmarker bench(5.0);
    auto results = bench.run_directory(dir);

    CHECK(!results.empty());

    // Verify CSV output has correct number of rows.
    auto csv = coso::Benchmarker::to_csv(results);
    // Count newlines: 1 header + N data rows.
    int newlines = 0;
    for (char c : csv) {
        if (c == '\n') ++newlines;
    }
    CHECK(newlines == static_cast<int>(results.size()) + 1);

    // Print summary for visibility in CI output.
    coso::Benchmarker::print_summary(results);
}

TEST_CASE("run_directory returns empty for nonexistent directory", "[benchmarker]")
{
    coso::Benchmarker bench(1.0);
    auto results = bench.run_directory("/tmp/nonexistent_coso_dir_12345");
    CHECK(results.empty());
}
