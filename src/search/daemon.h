#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/stop_criterion.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace coso {

/// Daemon mode: runs a solver continuously in a background thread,
/// accepting dynamic updates and providing thread-safe access to the
/// current best solution.
///
/// Usage:
///   Daemon daemon(data);
///   daemon.start(eval, stop);
///   // ... later:
///   daemon.update([](ProblemData::Builder& b) { /* modify */ });
///   auto sol = daemon.current_solution();
///   daemon.stop();
///
/// The solver runs an ILS+GA portfolio loop in the background.  When
/// update() is called, the solver pauses after its current iteration,
/// rebuilds ProblemData from the updated builder, and resumes solving
/// from a new initial solution.
///
/// Thread safety:
///   - start(), stop(), update(), current_solution() are all safe to
///     call from the main thread while the solver runs in the background.
///   - Only one background solver thread runs at a time.
class Daemon {
public:
    /// Construct a daemon for the given problem data.
    ///
    /// @param data  The compiled problem data.  A copy of the builder
    ///              state is stored internally for dynamic updates.
    explicit Daemon(ProblemData const& data);

    /// Destructor.  Stops the background solver if running.
    ~Daemon();

    // Non-copyable, non-movable (owns a thread).
    Daemon(Daemon const&) = delete;
    Daemon& operator=(Daemon const&) = delete;
    Daemon(Daemon&&) = delete;
    Daemon& operator=(Daemon&&) = delete;

    /// Start solving in a background thread.
    ///
    /// @param eval  Cost evaluator (penalty weights).
    /// @param stop  Stop criterion.  The daemon will stop when the
    ///              criterion triggers or stop() is called explicitly.
    void start(CostEvaluator const& eval, StopCriterion& stop);

    /// Apply a dynamic update to the problem data.
    ///
    /// The callback receives the current ProblemData and should return
    /// a new ProblemData with the desired changes.  The solver will
    /// pause, adopt the new data, and resume.
    ///
    /// This call blocks until the update has been adopted by the solver.
    ///
    /// @param transform  Function that takes the current ProblemData
    ///                   and returns the updated ProblemData.
    void update(std::function<ProblemData(ProblemData const&)> transform);

    /// Return the best solution found so far, or nullopt if no solution
    /// has been found yet (e.g., solver has not started).
    [[nodiscard]] std::optional<Solution> current_solution() const;

    /// Gracefully stop the background solver.
    ///
    /// Blocks until the solver thread has finished.  After this call,
    /// current_solution() still returns the last best solution.
    void stop();

    /// Whether the solver is currently running.
    [[nodiscard]] bool running() const noexcept;

private:
    /// The solver loop that runs in the background thread.
    void solver_loop_(CostEvaluator const& eval, StopCriterion& stop);

    // Current problem data (protected by mutex for updates).
    ProblemData data_;
    mutable std::mutex data_mutex_;

    // Best solution found so far.
    std::optional<Solution> best_solution_;
    mutable std::mutex solution_mutex_;

    // Pending update (set by update(), consumed by solver loop).
    std::function<ProblemData(ProblemData const&)> pending_update_;
    std::mutex update_mutex_;
    std::atomic<bool> has_pending_update_{false};
    std::atomic<bool> update_applied_{false};

    // Solver thread control.
    std::jthread solver_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
};

}  // namespace coso
