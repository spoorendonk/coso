#include "model/schedule_model.h"

#include "common/work_units.h"
#include "scheduling/construction.h"
#include "scheduling/parsers.h"
#include "scheduling/schedule_data.h"
#include "scheduling/schedule_solution.h"
#include "search/stop_criterion.h"

#include <chrono>
#include <limits>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Machine / Job / Operation registration
// ---------------------------------------------------------------------------

int ScheduleModel::add_machine(MachineParams p) {
    int idx = static_cast<int>(machines_.size());
    machines_.push_back(std::move(p));
    return idx;
}

int ScheduleModel::add_job(JobParams p) {
    int idx = static_cast<int>(jobs_.size());
    jobs_.push_back(std::move(p));
    job_operations_.emplace_back();  // empty operation list for this job
    return idx;
}

int ScheduleModel::add_operation(int job, OperationParams p) {
    if (job < 0 || job >= static_cast<int>(jobs_.size())) {
        throw std::out_of_range("ScheduleModel::add_operation: invalid job index");
    }

    int idx = static_cast<int>(operations_.size());
    operations_.push_back({.job = job, .params = std::move(p)});
    job_operations_[job].push_back(idx);
    return idx;
}

// ---------------------------------------------------------------------------
//  Objective
// ---------------------------------------------------------------------------

void ScheduleModel::set_objective(ScheduleObjective obj) {
    objective_ = obj;
}

void ScheduleModel::minimize_makespan() {
    objective_ = ScheduleObjective::Makespan;
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result ScheduleModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    // Validate: need at least one machine and one job.
    if (machines_.empty() || jobs_.empty()) {
        return {};  // cannot solve without machines/jobs
    }

    // -----------------------------------------------------------------------
    //  Build ScheduleData (compiled instance)
    // -----------------------------------------------------------------------

    ScheduleData::Builder builder;

    for (auto const& m : machines_) {
        builder.add_machine(m);
        work.count(1);
    }

    for (auto const& j : jobs_) {
        builder.add_job(j);
        work.count(1);
    }

    for (auto const& op : operations_) {
        builder.add_operation(op.job, op.params);
        work.count(2);
    }

    builder.set_objective(objective_);
    work.count(1);

    ScheduleData data = builder.build();
    work.count(static_cast<uint64_t>(data.num_operations()) +
               static_cast<uint64_t>(data.num_machines()));
    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    auto evaluate_candidate = [&](Result& candidate) {
        if (candidate.schedule_.size() != static_cast<size_t>(data.num_operations())) {
            candidate.feasible_ = false;
            candidate.cost_ = std::numeric_limits<double>::infinity();
            return;
        }

        ScheduleSolution sol(data);
        for (int op = 0; op < data.num_operations(); ++op) {
            auto const& s = candidate.schedule_[op];
            if (s.machine < 0 || s.machine >= data.num_machines()) {
                candidate.feasible_ = false;
                candidate.cost_ = std::numeric_limits<double>::infinity();
                return;
            }
            sol.assign(op, s.machine, s.start_time);
        }

        candidate.feasible_ = sol.feasible();
        candidate.makespan_ = sol.makespan();
        candidate.cost_ = static_cast<double>(sol.objective());
    };

    auto better = [](Result const& a, Result const& b) {
        if (a.feasible_ != b.feasible_) {
            return a.feasible_;
        }
        return a.cost_ < b.cost_;
    };

    Result best;
    bool have_best = false;
    auto consider = [&](Result candidate) {
        evaluate_candidate(candidate);
        work.count(static_cast<uint64_t>(data.num_operations()));
        if (!have_best || better(candidate, best)) {
            best = std::move(candidate);
            have_best = true;
        }
    };

    consider(construct_sgs(data));
    work.count(3);

    if (!stop.should_stop()) {
        consider(construct_dispatch(data, DispatchRule::SPT));
        work.count(2);
    }
    if (!stop.should_stop()) {
        consider(construct_neh(data));
        work.count(2);
    }

    Result result = have_best ? best : Result{};
    result.iterations_ = have_best ? 1 : 0;
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

// ---------------------------------------------------------------------------
//  Free function: solve from JSP file (Taillard format)
// ---------------------------------------------------------------------------

Result solve_jsp(const std::string& instance_path, TimeLimit /*tl*/) {
    try {
        // Keep solve_jsp as a convenience wrapper around parsed data.
        // For baseline behavior we use dispatch construction.
        ScheduleData data = read_taillard_jsp(instance_path);
        Result result = construct_dispatch(data, DispatchRule::SPT);
        ScheduleSolution sol(data);
        for (int op = 0; op < data.num_operations(); ++op) {
            auto const& s = result.schedule_[op];
            sol.assign(op, s.machine, s.start_time);
        }
        result.feasible_ = sol.feasible();
        result.makespan_ = sol.makespan();
        result.cost_ = static_cast<double>(sol.objective());
        return result;
    } catch (...) {
        return {};
    }
}

}  // namespace coso
