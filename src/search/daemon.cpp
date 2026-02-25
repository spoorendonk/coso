#include "search/daemon.h"

#include "search/iterated_local_search.h"
#include "search/solution_finalizer.h"
#include "search/stop_criterion.h"

namespace coso {

Daemon::Daemon(ProblemData const& data)
    : data_(data)
{
}

Daemon::~Daemon()
{
    stop();
}

void Daemon::start(CostEvaluator const& eval, StopCriterion& stop)
{
    if (running_.load())
        return;  // already running

    stop_requested_.store(false);
    running_.store(true);

    solver_thread_ = std::jthread([this, &eval, &stop] {
        solver_loop_(eval, stop);
    });
}

void Daemon::update(std::function<ProblemData(ProblemData const&)> transform)
{
    if (!running_.load())
        return;

    // Set the pending update.
    {
        std::lock_guard lock(update_mutex_);
        pending_update_ = std::move(transform);
        update_applied_.store(false);
        has_pending_update_.store(true);
    }

    // Wait for the solver loop to apply the update.
    while (!update_applied_.load() && running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::optional<Solution> Daemon::current_solution() const
{
    std::lock_guard lock(solution_mutex_);
    return best_solution_;
}

void Daemon::stop()
{
    stop_requested_.store(true);

    if (solver_thread_.joinable()) {
        solver_thread_.join();
    }

    running_.store(false);
}

bool Daemon::running() const noexcept
{
    return running_.load();
}

void Daemon::solver_loop_(CostEvaluator const& eval, StopCriterion& stop)
{
    // Each "epoch" runs ILS for a short iteration budget, then checks
    // for updates and stop conditions.
    constexpr int kEpochIterations = 50;

    ProblemData current_data = [&] {
        std::lock_guard lock(data_mutex_);
        return data_;
    }();

    while (!stop_requested_.load() && !stop.should_stop()) {
        // Run a short ILS epoch.
        StopCriterion epoch_stop(0.0, kEpochIterations, 0);
        IteratedLocalSearch ils(current_data, 42);
        Solution epoch_sol = ils.run(eval, epoch_stop);

        // Update iteration count on the parent stop criterion.
        for (int i = 0; i < epoch_stop.iterations(); ++i) {
            stop.iteration();
        }

        // Update best solution if this epoch found something better.
        {
            std::lock_guard lock(solution_mutex_);
            if (!best_solution_.has_value() ||
                epoch_sol.cost(eval) < best_solution_->cost(eval)) {
                best_solution_ = std::move(epoch_sol);
                stop.improvement();
            }
        }

        // Check for pending updates.
        if (has_pending_update_.load()) {
            std::function<ProblemData(ProblemData const&)> transform;
            {
                std::lock_guard lock(update_mutex_);
                transform = std::move(pending_update_);
                has_pending_update_.store(false);
            }

            // Apply the update.
            ProblemData new_data = transform(current_data);

            {
                std::lock_guard lock(data_mutex_);
                data_ = new_data;
            }
            current_data = std::move(new_data);

            // Clear best solution since data changed -- next epoch will
            // produce a new solution for the updated data.
            {
                std::lock_guard lock(solution_mutex_);
                best_solution_.reset();
            }

            update_applied_.store(true);
        }
    }

    // Finalize best solution before exiting.
    {
        std::lock_guard lock(solution_mutex_);
        if (best_solution_.has_value()) {
            SolutionFinalizer finalizer(current_data);
            finalizer.finalize(*best_solution_, &stop);
        }
    }

    running_.store(false);
}

} // namespace coso
