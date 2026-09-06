#pragma once

#include "types.h"

#include <climits>
#include <string>
#include <vector>

namespace coso {

/// Parameters for a machine.
struct MachineParams {
    std::string name;
};

/// Parameters for an operation within a job.
struct OperationParams {
    int machine = -1;                        ///< fixed machine (-1 = flexible / FJSP)
    std::vector<int> eligible_machines;      ///< for FJSP: machine alternatives
    std::vector<int> durations_per_machine;  ///< duration on each eligible machine
    int duration = 0;                        ///< fixed duration (when machine is fixed)
    bool optional = false;
};

/// Parameters for a job.
struct JobParams {
    std::string name;
    int release_time = 0;
    int due_date = INT_MAX;
    int weight = 1;  ///< for weighted tardiness objectives
};

/// Scheduling objective function.
enum class ScheduleObjective {
    Makespan,                ///< minimize max completion time
    TotalWeightedTardiness,  ///< minimize sum of w_j * max(0, C_j - d_j)
    TotalFlowTime,           ///< minimize sum of C_j
};

/// Scheduling model: declare machines, jobs, operations, then solve.
///
/// Supports JSP, FJSP, RCPSP, and related scheduling problems.
class ScheduleModel {
public:
    // -- Stored entry types --------------------------------------------------

    /// An operation as declared, with the job it belongs to.
    struct OperationEntry {
        int job = -1;
        OperationParams params;
    };

    /// A precedence constraint declared beyond the default intra-job ordering.
    struct Precedence {
        int before;
        int after;
    };

    /// Add a machine with the given parameters.
    int add_machine(MachineParams p = {});

    /// Add a job with the given parameters.
    int add_job(JobParams p = {});

    /// Add an operation to a job.
    int add_operation(int job, OperationParams p);

    // -- Resource constraints (RCPSP) ----------------------------------------

    /// Add a renewable resource with the given capacity.
    int add_resource(int capacity);

    /// Set the resource usage for an operation.
    void set_resource_usage(int operation, int resource, int amount);

    // -- Precedence ----------------------------------------------------------

    /// Add a precedence constraint (beyond default intra-job ordering).
    void add_precedence(int op_before, int op_after);

    // -- Objective ------------------------------------------------------------

    /// Set the scheduling objective (default: Makespan).
    void set_objective(ScheduleObjective obj);

    /// Convenience: set objective to minimize makespan.
    void minimize_makespan();

    // -- Warm start ----------------------------------------------------------

    /// Provide an initial schedule: per-operation (machine, start_time) pairs.
    void set_initial_schedule(const std::vector<std::pair<int, int>>& op_assignments);

    // -- Solve ---------------------------------------------------------------

    /// Solve the scheduling problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_machines() const noexcept { return static_cast<int>(machines_.size()); }
    [[nodiscard]] MachineParams const& machine(int m) const { return machines_[m]; }

    [[nodiscard]] int num_jobs() const noexcept { return static_cast<int>(jobs_.size()); }
    [[nodiscard]] JobParams const& job(int j) const { return jobs_[j]; }

    [[nodiscard]] int num_operations() const noexcept {
        return static_cast<int>(operations_.size());
    }
    [[nodiscard]] OperationEntry const& operation(int o) const { return operations_[o]; }

    /// Operation ids per job, job_operations()[job], in declaration order.
    [[nodiscard]] auto const& job_operations() const noexcept { return job_operations_; }

    [[nodiscard]] int num_resources() const noexcept {
        return static_cast<int>(resource_capacities_.size());
    }
    [[nodiscard]] auto const& resource_capacities() const noexcept { return resource_capacities_; }

    /// Per-operation resource usage exactly as stored: resource_usage()[op] is
    /// ragged, grown only as far as the highest resource set for that
    /// operation.  A row shorter than num_resources() — an empty row
    /// included — means the missing resources are unset, i.e. 0.  Nothing here
    /// is padded.
    [[nodiscard]] auto const& resource_usage() const noexcept { return resource_usage_; }

    /// Precedences added by add_precedence(), in declaration order.
    [[nodiscard]] auto const& extra_precedences() const noexcept { return extra_precedences_; }

    /// Per-operation (machine, start_time) pairs from set_initial_schedule().
    [[nodiscard]] auto const& initial_schedule() const noexcept { return initial_schedule_; }

    [[nodiscard]] ScheduleObjective objective() const noexcept { return objective_; }

private:
    // -- Stored machine data -------------------------------------------------
    std::vector<MachineParams> machines_;

    // -- Stored job data -----------------------------------------------------
    std::vector<JobParams> jobs_;

    // -- Stored operation data -----------------------------------------------
    std::vector<OperationEntry> operations_;

    // -- Job → operations mapping --------------------------------------------
    std::vector<std::vector<int>> job_operations_;

    // -- Resources (RCPSP) ---------------------------------------------------
    std::vector<int> resource_capacities_;

    /// Per-operation resource usage: resource_usage_[op][res] = amount.
    std::vector<std::vector<int>> resource_usage_;

    // -- Additional precedence constraints -----------------------------------
    std::vector<Precedence> extra_precedences_;

    // -- Objective -----------------------------------------------------------
    ScheduleObjective objective_ = ScheduleObjective::Makespan;

    // -- Warm start ----------------------------------------------------------
    std::vector<std::pair<int, int>> initial_schedule_;
};

/// Convenience: solve a JSP instance file (Taillard format) directly.
Result solve_jsp(const std::string& instance_path, TimeLimit tl);

}  // namespace coso
