#pragma once

#include "routing/cost_evaluator.h"

#include <algorithm>
#include <cmath>

namespace coso {

/// Adaptive penalty weight manager.
///
/// Tracks the fraction of feasible solutions over a sliding window and
/// adjusts penalty weights to steer toward a target feasibility ratio.
///
/// Adjustment rule (applied every `update_interval` registrations):
///   - If feasible fraction > target: penalties decrease (allow more
///     infeasible exploration).
///   - If feasible fraction < target: penalties increase (push toward
///     feasibility).
///   - Multiplicative update: weight *= (1 + rate) or weight *= (1 - rate).
///
/// Maintains independent weights for load, time-window, and distance
/// penalty dimensions.
class PenaltyManager {
public:
    /// @param target_feasible  Desired fraction of feasible solutions (0..1).
    /// @param adjustment_rate  Multiplicative adjustment step (e.g. 0.1 = 10%).
    /// @param update_interval  Number of registrations between weight updates.
    explicit PenaltyManager(double target_feasible = 0.5,
                            double adjustment_rate = 0.1,
                            int update_interval = 100);

    /// Register a solution's feasibility status.
    ///
    /// After every `update_interval` calls, penalties are adjusted.
    void register_solution(bool feasible);

    /// Return a CostEvaluator with the current penalty weights.
    [[nodiscard]] CostEvaluator cost_evaluator() const;

    /// Current penalty weights.
    [[nodiscard]] double load_penalty() const noexcept { return load_pen_; }
    [[nodiscard]] double tw_penalty() const noexcept { return tw_pen_; }
    [[nodiscard]] double dist_penalty() const noexcept { return dist_pen_; }

    /// Number of solutions registered so far.
    [[nodiscard]] int num_registered() const noexcept { return total_; }

    /// Current feasible fraction in the active window.
    [[nodiscard]] double feasible_fraction() const noexcept {
        return window_count_ > 0
            ? static_cast<double>(window_feasible_) / window_count_
            : 0.0;
    }

private:
    double target_feasible_;
    double adjustment_rate_;
    int update_interval_;

    // Penalty weights (double for smooth adjustment, cast to int for CostEvaluator).
    double load_pen_ = 100.0;
    double tw_pen_   = 100.0;
    double dist_pen_ = 100.0;

    // Sliding window counters (reset each update interval).
    int window_count_    = 0;
    int window_feasible_ = 0;
    int total_           = 0;

    static constexpr double kMinPenalty = 1.0;
    static constexpr double kMaxPenalty = 100000.0;

    void update_penalties_();
};

} // namespace coso
