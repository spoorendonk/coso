#pragma once

#include "scheduling/schedule_data.h"

#include <cassert>
#include <climits>
#include <vector>

namespace coso {

/// Mutable schedule solution: tracks operation-to-(machine, start_time)
/// assignments for JSP and FJSP problems.
///
/// Provides objective evaluation (makespan, weighted tardiness, flow time)
/// and feasibility checking (no machine overlaps, precedences respected).
class ScheduleSolution {
public:
    /// Per-operation assignment.
    struct Assignment {
        int machine    = -1;  ///< assigned machine (-1 = unassigned)
        int start_time = -1;  ///< start time (-1 = unassigned)

        [[nodiscard]] bool assigned() const noexcept {
            return machine >= 0 && start_time >= 0;
        }
    };

    /// Construct an empty solution for the given problem data.
    explicit ScheduleSolution(ScheduleData const& data);

    // -------------------------------------------------------------------
    //  Mutation
    // -------------------------------------------------------------------

    /// Assign an operation to a machine at a given start time.
    void assign(int op, int machine, int start_time);

    /// Remove the assignment for an operation.
    void unassign(int op);

    // -------------------------------------------------------------------
    //  Objective values
    // -------------------------------------------------------------------

    /// Makespan: max completion time over all assigned operations.
    [[nodiscard]] int makespan() const;

    /// Total weighted tardiness: sum of w_j * max(0, C_j - d_j).
    /// C_j = completion time of last operation in job j.
    [[nodiscard]] int total_weighted_tardiness() const;

    /// Total flow time: sum of C_j over all jobs (completion of last op).
    [[nodiscard]] int total_flow_time() const;

    /// Evaluate the objective function configured in the ScheduleData.
    [[nodiscard]] int objective() const;

    // -------------------------------------------------------------------
    //  Feasibility
    // -------------------------------------------------------------------

    /// Check overall feasibility: all operations assigned, no overlaps,
    /// precedences respected, machine eligibility correct.
    [[nodiscard]] bool feasible() const;

    /// Check that no two operations overlap on any machine.
    [[nodiscard]] bool no_machine_overlaps() const;

    /// Check that all precedence constraints are respected.
    [[nodiscard]] bool precedences_respected() const;

    /// Check that all operations are assigned.
    [[nodiscard]] bool all_assigned() const;

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    [[nodiscard]] ScheduleData const& data() const noexcept { return data_; }

    [[nodiscard]] Assignment const& assignment(int op) const {
        assert(op >= 0 && op < static_cast<int>(assignments_.size()));
        return assignments_[op];
    }

    /// Completion time of an operation (start_time + processing_time).
    /// Returns -1 if the operation is unassigned.
    [[nodiscard]] int completion_time(int op) const;

    /// Completion time of the last operation in a job.
    /// Returns -1 if any operation in the job is unassigned.
    [[nodiscard]] int job_completion_time(int job) const;

    /// Operations assigned to a machine, sorted by start time.
    [[nodiscard]] std::vector<int> machine_operations(int machine) const;

    /// Number of assigned operations.
    [[nodiscard]] int num_assigned() const noexcept { return num_assigned_; }

private:
    ScheduleData const& data_;
    std::vector<Assignment> assignments_;
    int num_assigned_ = 0;
};

} // namespace coso
