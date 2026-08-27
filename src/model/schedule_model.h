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

private:
    // -- Stored machine data -------------------------------------------------
    std::vector<MachineParams> machines_;

    // -- Stored job data -----------------------------------------------------
    std::vector<JobParams> jobs_;

    // -- Stored operation data -----------------------------------------------
    struct OperationEntry {
        int job = -1;
        OperationParams params;
    };
    std::vector<OperationEntry> operations_;

    // -- Job → operations mapping --------------------------------------------
    std::vector<std::vector<int>> job_operations_;

    // -- Resources (RCPSP) ---------------------------------------------------
    std::vector<int> resource_capacities_;

    /// Per-operation resource usage: resource_usage_[op][res] = amount.
    std::vector<std::vector<int>> resource_usage_;

    // -- Additional precedence constraints -----------------------------------
    struct Precedence {
        int before;
        int after;
    };
    std::vector<Precedence> extra_precedences_;

    // -- Objective -----------------------------------------------------------
    ScheduleObjective objective_ = ScheduleObjective::Makespan;

    // -- Warm start ----------------------------------------------------------
    std::vector<std::pair<int, int>> initial_schedule_;
};

/// Convenience: solve a JSP instance file (Taillard format) directly.
Result solve_jsp(const std::string& instance_path, TimeLimit tl);

}  // namespace coso
