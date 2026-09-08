#pragma once

#include "types.h"

#include <climits>
#include <stdexcept>
#include <string>
#include <vector>

namespace coso {

/// Parameters for a machine. Machines are identified by index alone.
struct MachineParams {};

/// Parameters for an operation within a job.
struct OperationParams {
    int machine = -1;                        ///< fixed machine (-1 = flexible / FJSP)
    std::vector<int> eligible_machines;      ///< for FJSP: machine alternatives
    std::vector<int> durations_per_machine;  ///< duration on each eligible machine
    int duration = 0;                        ///< fixed duration (when machine is fixed)
};

/// Parameters for a job.
struct JobParams {
    int due_date = INT_MAX;
    int weight = 1;  ///< for weighted tardiness objectives
};

/// Scheduling objective function.
enum class ScheduleObjective {
    Makespan,                ///< minimize max completion time
    TotalWeightedTardiness,  ///< minimize sum of w_j * max(0, C_j - d_j)
};

/// Scheduling model: declare machines, jobs, operations, then solve.
///
/// Supports the shop archetype: JSP, FJSP and single-machine problems.
class ScheduleModel {
public:
    // -- Stored entry types --------------------------------------------------

    /// An operation as declared, with the job it belongs to.
    struct OperationEntry {
        int job = -1;
        OperationParams params;
    };

    /// Add a machine with the given parameters.
    int add_machine(MachineParams p = {});

    /// Add a job with the given parameters.
    int add_job(JobParams p = {});

    /// Add an operation to a job.
    int add_operation(int job, OperationParams p);

    // -- Objective ------------------------------------------------------------

    /// Set the scheduling objective (default: Makespan).
    void set_objective(ScheduleObjective obj);

    /// Convenience: set objective to minimize makespan.
    void minimize_makespan();

    // -- Solve ---------------------------------------------------------------

    /// Solve the scheduling problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_machines() const noexcept { return static_cast<int>(machines_.size()); }
    [[nodiscard]] MachineParams const& machine(int m) const {
        if (m < 0 || static_cast<size_t>(m) >= machines_.size()) {
            throw std::out_of_range("ScheduleModel::machine: invalid index");
        }
        return machines_[m];
    }

    [[nodiscard]] int num_jobs() const noexcept { return static_cast<int>(jobs_.size()); }
    [[nodiscard]] JobParams const& job(int j) const {
        if (j < 0 || static_cast<size_t>(j) >= jobs_.size()) {
            throw std::out_of_range("ScheduleModel::job: invalid index");
        }
        return jobs_[j];
    }

    [[nodiscard]] int num_operations() const noexcept {
        return static_cast<int>(operations_.size());
    }
    [[nodiscard]] OperationEntry const& operation(int o) const {
        if (o < 0 || static_cast<size_t>(o) >= operations_.size()) {
            throw std::out_of_range("ScheduleModel::operation: invalid index");
        }
        return operations_[o];
    }

    /// Operation ids per job, job_operations()[job], in declaration order.
    [[nodiscard]] auto const& job_operations() const noexcept { return job_operations_; }

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

    // -- Objective -----------------------------------------------------------
    ScheduleObjective objective_ = ScheduleObjective::Makespan;
};

/// Convenience: solve a JSP instance file (Taillard format) directly.
Result solve_jsp(const std::string& instance_path, TimeLimit tl);

}  // namespace coso
