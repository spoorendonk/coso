#include "model/schedule_model.h"
#include "common/work_units.h"
#include "search/stop_criterion.h"
#include "scheduling/schedule_data.h"

#include <chrono>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Machine / Job / Operation registration
// ---------------------------------------------------------------------------

int ScheduleModel::add_machine(MachineParams p)
{
    int idx = static_cast<int>(machines_.size());
    machines_.push_back(std::move(p));
    return idx;
}

int ScheduleModel::add_job(JobParams p)
{
    int idx = static_cast<int>(jobs_.size());
    jobs_.push_back(std::move(p));
    job_operations_.emplace_back();  // empty operation list for this job
    return idx;
}

int ScheduleModel::add_operation(int job, OperationParams p)
{
    if (job < 0 || job >= static_cast<int>(jobs_.size()))
        throw std::out_of_range("ScheduleModel::add_operation: invalid job index");

    int idx = static_cast<int>(operations_.size());
    operations_.push_back({.job = job, .params = std::move(p)});
    job_operations_[job].push_back(idx);

    // Extend resource_usage_ to accommodate the new operation.
    resource_usage_.emplace_back();
    return idx;
}

// ---------------------------------------------------------------------------
//  Resources (RCPSP)
// ---------------------------------------------------------------------------

int ScheduleModel::add_resource(int capacity)
{
    int idx = static_cast<int>(resource_capacities_.size());
    resource_capacities_.push_back(capacity);
    return idx;
}

void ScheduleModel::set_resource_usage(int operation, int resource, int amount)
{
    if (operation < 0 || operation >= static_cast<int>(operations_.size()))
        throw std::out_of_range("ScheduleModel::set_resource_usage: invalid operation");
    if (resource < 0 || resource >= static_cast<int>(resource_capacities_.size()))
        throw std::out_of_range("ScheduleModel::set_resource_usage: invalid resource");

    auto& usage = resource_usage_[operation];
    if (static_cast<int>(usage.size()) <= resource)
        usage.resize(resource + 1, 0);
    usage[resource] = amount;
}

// ---------------------------------------------------------------------------
//  Precedence
// ---------------------------------------------------------------------------

void ScheduleModel::add_precedence(int op_before, int op_after)
{
    extra_precedences_.push_back({op_before, op_after});
}

// ---------------------------------------------------------------------------
//  Objective
// ---------------------------------------------------------------------------

void ScheduleModel::set_objective(ScheduleObjective obj)
{
    objective_ = obj;
}

void ScheduleModel::minimize_makespan()
{
    objective_ = ScheduleObjective::Makespan;
}

// ---------------------------------------------------------------------------
//  Warm start
// ---------------------------------------------------------------------------

void ScheduleModel::set_initial_schedule(
    const std::vector<std::pair<int, int>>& op_assignments)
{
    initial_schedule_ = op_assignments;
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result ScheduleModel::solve(TimeLimit tl)
{
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

    for (int r = 0; r < static_cast<int>(resource_capacities_.size()); ++r) {
        builder.add_resource(resource_capacities_[r]);
        work.count(1);
    }

    // Set resource usage.
    for (int o = 0; o < static_cast<int>(resource_usage_.size()); ++o) {
        for (int r = 0; r < static_cast<int>(resource_usage_[o].size()); ++r) {
            work.count(1);
            if (resource_usage_[o][r] != 0) {
                builder.set_resource_usage(o, r, resource_usage_[o][r]);
                work.count(1);
            }
        }
    }

    // Extra precedences.
    for (auto const& p : extra_precedences_) {
        builder.add_precedence(p.before, p.after);
        work.count(1);
    }

    builder.set_objective(objective_);
    work.count(1);

    [[maybe_unused]] ScheduleData data = builder.build();
    work.count(static_cast<uint64_t>(data.num_operations())
             + static_cast<uint64_t>(data.num_machines()));
    (void)stop.should_stop();

    // -----------------------------------------------------------------------
    //  Stub: no solver yet (comes in work unit 7.4+).
    //  Return an infeasible result with elapsed time.
    // -----------------------------------------------------------------------

    Result result;
    result.feasible_ = false;
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(
        wall_end - wall_start).count();

    return result;
}

// ---------------------------------------------------------------------------
//  Free function: solve from JSP file (Taillard format)
// ---------------------------------------------------------------------------

Result solve_jsp(const std::string& /*instance_path*/, TimeLimit /*tl*/)
{
    // Stub: JSP file reader will come in a later work unit.
    Result result;
    result.work_ticks_ = 0;
    result.work_units_ = 0.0;
    return result;
}

} // namespace coso
