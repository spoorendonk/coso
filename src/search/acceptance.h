#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <variant>
#include <vector>

namespace coso {

/// Late Acceptance Hill Climbing (LAHC).
///
/// Accepts a candidate if its cost is at most the cost recorded L iterations
/// ago, or at most the current solution cost.  The fitness list is initialized
/// to the cost of the initial solution.
class LateAcceptance {
public:
    /// @param list_length  Number of entries in the fitness list (default 5000).
    explicit LateAcceptance(int list_length = 5000);

    /// Initialize the fitness list to the given starting cost.
    void init(int64_t cost);

    /// Whether to accept the candidate solution.
    [[nodiscard]] bool accept(int64_t candidate_cost,
                              int64_t current_cost);

    /// Advance one iteration (updates the fitness list with current cost).
    void iteration(int64_t current_cost);

    [[nodiscard]] int list_length() const noexcept { return list_length_; }

private:
    int list_length_;
    std::vector<int64_t> list_;
    int index_ = 0;
};

/// Simulated Annealing.
///
/// Accepts worse solutions with probability exp(-delta / T), where delta is
/// the cost increase and T is a temperature that decays geometrically each
/// iteration: T *= alpha.
class SimulatedAnnealing {
public:
    /// @param initial_temp  Starting temperature.
    /// @param alpha         Cooling factor per iteration (0 < alpha < 1).
    /// @param seed          Random seed.
    SimulatedAnnealing(double initial_temp, double alpha,
                       unsigned int seed = 42);

    /// Initialize (resets temperature to initial value).
    void init(int64_t cost);

    /// Whether to accept the candidate solution.
    [[nodiscard]] bool accept(int64_t candidate_cost,
                              int64_t current_cost);

    /// Advance one iteration (cools temperature).
    void iteration(int64_t current_cost);

    [[nodiscard]] double temperature() const noexcept { return temp_; }

private:
    double initial_temp_;
    double alpha_;
    double temp_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};

/// Record-to-Record Travel.
///
/// Accepts a candidate if its cost is within a threshold of the best cost
/// seen so far.  The threshold decays linearly each iteration.
class RecordToRecord {
public:
    /// @param initial_threshold  Starting threshold (absolute).
    /// @param decay              Amount to subtract from threshold each
    ///                           iteration.
    RecordToRecord(double initial_threshold, double decay);

    /// Initialize (resets threshold and records starting cost as best).
    void init(int64_t cost);

    /// Whether to accept the candidate solution.
    [[nodiscard]] bool accept(int64_t candidate_cost,
                              int64_t current_cost);

    /// Advance one iteration (decays threshold).
    void iteration(int64_t current_cost);

    [[nodiscard]] double threshold() const noexcept { return threshold_; }

private:
    double initial_threshold_;
    double decay_;
    double threshold_;
    int64_t best_cost_;
};

/// A composable acceptance criterion.
///
/// Wraps one of the concrete acceptance strategies using std::variant.
/// This avoids virtual function overhead while still allowing runtime
/// selection of the acceptance criterion.
class AcceptanceCriterion {
public:
    /// Construct from any concrete acceptance strategy.
    AcceptanceCriterion(LateAcceptance la);
    AcceptanceCriterion(SimulatedAnnealing sa);
    AcceptanceCriterion(RecordToRecord rtr);

    /// Initialize the criterion with the starting solution cost.
    void init(int64_t cost);

    /// Whether to accept the candidate solution.
    [[nodiscard]] bool accept(int64_t candidate_cost,
                              int64_t current_cost);

    /// Advance one iteration.
    void iteration(int64_t current_cost);

private:
    std::variant<LateAcceptance, SimulatedAnnealing, RecordToRecord> impl_;
};

} // namespace coso
