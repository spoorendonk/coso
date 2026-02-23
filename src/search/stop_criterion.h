#pragma once

#include <chrono>
#include <limits>

namespace coso {

/// Stop criterion for iterative search algorithms.
///
/// Supports three independent stopping conditions:
///   1. Wall-clock time limit (seconds)
///   2. Maximum number of iterations
///   3. Maximum iterations without improvement to the best solution
///
/// The search should stop when **any** of the active criteria triggers.
/// Call iteration() after each iteration and improvement() when the best
/// solution improves.  should_stop() checks all active criteria.
class StopCriterion {
public:
    /// Construct with a time limit only (other criteria inactive).
    explicit StopCriterion(double time_limit_s);

    /// Construct with all three criteria.
    ///
    /// @param time_limit_s    Wall-clock time limit in seconds (0 = no limit).
    /// @param max_iter        Maximum iterations (0 = no limit).
    /// @param max_no_improve  Maximum iterations without improvement (0 = no limit).
    StopCriterion(double time_limit_s, int max_iter, int max_no_improve);

    /// Check whether any stopping condition has been met.
    [[nodiscard]] bool should_stop() const;

    /// Notify that one iteration has completed.
    void iteration();

    /// Notify that the best solution has improved.
    void improvement();

    /// Current iteration count.
    [[nodiscard]] int iterations() const noexcept { return iter_; }

    /// Iterations since last improvement.
    [[nodiscard]] int iterations_no_improve() const noexcept {
        return iter_ - last_improve_iter_;
    }

    /// Elapsed wall-clock time in seconds since construction.
    [[nodiscard]] double elapsed() const;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point start_;
    double time_limit_s_;
    int max_iter_;
    int max_no_improve_;

    int iter_ = 0;
    int last_improve_iter_ = 0;
};

} // namespace coso
