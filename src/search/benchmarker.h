#pragma once

#include "model/types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace coso {

/// Result of benchmarking a single VRP instance.
struct BenchmarkResult {
    std::string instance;    ///< Instance filename (without path).
    int bks = 0;             ///< Best known solution cost.
    int64_t cost = 0;        ///< Solver cost.
    double gap_pct = 0.0;    ///< Gap to BKS as percentage: (cost - bks) / bks * 100.
    double elapsed_s = 0.0;  ///< Wall-clock time in seconds.
    double work_units = 0.0; ///< Deterministic work consumed.
    uint64_t work_ticks = 0; ///< Raw deterministic ticks consumed.
    bool feasible = false;   ///< Whether the solution is feasible.
    int num_routes = 0;      ///< Number of routes in the solution.
};

/// Automated benchmark runner for CVRP instances.
///
/// Runs a solver on each instance in a directory, records results, and
/// produces CSV output and summary statistics.
class Benchmarker {
public:
    /// @param time_limit_s  Time limit per instance in seconds.
    explicit Benchmarker(double time_limit_s = 60.0);

    /// Run a single instance with a known BKS value.
    BenchmarkResult run_instance(std::string const& path, int bks);

    /// Run all .vrp instances in a directory.
    ///
    /// Uses built-in BKS values for known instances (Uchoa X-set).
    /// Unknown instances are run with BKS = 0 (gap will be 0%).
    std::vector<BenchmarkResult> run_directory(std::string const& dir);

    /// Convert results to CSV format (with header row).
    static std::string to_csv(std::vector<BenchmarkResult> const& results);

    /// Print a human-readable summary to stdout.
    static void print_summary(std::vector<BenchmarkResult> const& results);

    /// Look up the BKS for a known instance filename.
    /// Returns 0 if the instance is not in the table.
    static int lookup_bks(std::string const& filename);

private:
    double time_limit_s_;

    /// Built-in BKS table for Uchoa X-set instances.
    static std::unordered_map<std::string, int> const& bks_table();
};

} // namespace coso
