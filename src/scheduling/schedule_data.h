#pragma once

#include "model/schedule_model.h"
#include "scheduling/calendar.h"
#include "scheduling/setup_times.h"

#include <cassert>
#include <climits>
#include <optional>
#include <span>
#include <vector>

namespace coso {

/// Compiled, immutable representation of a scheduling instance.
///
/// Built once from a ScheduleModel. Provides efficient, cache-friendly access
/// patterns for the scheduling solver engine:
///   - Operation-to-job and job-to-operations mappings
///   - Processing time matrix (operation x machine, for FJSP)
///   - Precedence DAG (topological structure)
///   - Objective type and job parameters
class ScheduleData {
public:
    // -------------------------------------------------------------------
    //  Operation data
    // -------------------------------------------------------------------

    struct OperationData {
        int job = -1;                            ///< owning job index
        int fixed_machine = -1;                  ///< -1 = flexible (FJSP)
        int duration = 0;                        ///< fixed duration (when machine is fixed)
        std::vector<int> eligible_machines;      ///< for FJSP
        std::vector<int> durations_per_machine;  ///< parallel to eligible_machines
    };

    // -------------------------------------------------------------------
    //  Job data
    // -------------------------------------------------------------------

    struct JobData {
        int due_date = INT_MAX;
        int weight = 1;
        std::vector<int> operations;  ///< operation indices in this job (ordered)
    };

    // -------------------------------------------------------------------
    //  Precedence arc
    // -------------------------------------------------------------------

    struct PrecedenceArc {
        int before;
        int after;
    };

    // -------------------------------------------------------------------
    //  Builder — the only way to construct a ScheduleData
    // -------------------------------------------------------------------

    class Builder {
    public:
        /// Add a machine. Returns machine index (0-based).
        int add_machine(MachineParams p = {});

        /// Add a job. Returns job index (0-based).
        int add_job(JobParams p = {});

        /// Add an operation to a job. Returns operation index (0-based, global).
        int add_operation(int job, OperationParams p);

        /// Set a sequence-dependent setup time: from_op -> to_op on machine.
        void set_setup_time(int from_op, int to_op, int machine, int time);

        /// Set a uniform setup time from from_op -> to_op on all machines.
        void set_setup_time(int from_op, int to_op, int time);

        /// Add an availability window for a machine.
        void add_machine_available(int machine, int start, int end);

        /// Set the scheduling objective.
        void set_objective(ScheduleObjective obj);

        /// Build the immutable ScheduleData.
        [[nodiscard]] ScheduleData build() const;

    private:
        std::vector<MachineParams> machines_;
        std::vector<JobParams> jobs_;

        struct OpEntry {
            int job = -1;
            OperationParams params;
        };
        std::vector<OpEntry> operations_;

        /// job -> list of operation indices
        std::vector<std::vector<int>> job_operations_;

        ScheduleObjective objective_ = ScheduleObjective::Makespan;

        /// Setup time entries (deferred until build).
        struct SetupEntry {
            int from;
            int to;
            int machine;
            int time;
        };
        std::vector<SetupEntry> setup_entries_;
        bool setup_uniform_ = false;
        struct SetupUniformEntry {
            int from;
            int to;
            int time;
        };
        std::vector<SetupUniformEntry> setup_uniform_entries_;

        /// Calendar entries (deferred until build).
        struct CalendarEntry {
            int machine;
            int start;
            int end;
        };
        std::vector<CalendarEntry> calendar_entries_;
    };

    // -------------------------------------------------------------------
    //  Accessors (all const — ScheduleData is immutable after construction)
    // -------------------------------------------------------------------

    [[nodiscard]] int num_machines() const noexcept { return num_machines_; }
    [[nodiscard]] int num_jobs() const noexcept { return num_jobs_; }
    [[nodiscard]] int num_operations() const noexcept { return num_operations_; }

    /// Operation data for operation index o (0-based).
    [[nodiscard]] OperationData const& operation(int o) const {
        assert(o >= 0 && o < num_operations_);
        return operations_[o];
    }

    /// Job data for job index j (0-based).
    [[nodiscard]] JobData const& job(int j) const {
        assert(j >= 0 && j < num_jobs_);
        return jobs_[j];
    }

    /// All precedence arcs (intra-job).
    [[nodiscard]] std::span<PrecedenceArc const> precedences() const noexcept {
        return precedences_;
    }

    /// The scheduling objective.
    [[nodiscard]] ScheduleObjective objective() const noexcept { return objective_; }

    /// Whether setup times are defined.
    [[nodiscard]] bool has_setup_times() const noexcept { return setup_times_.has_value(); }

    /// Setup time matrix (only valid if has_setup_times() is true).
    [[nodiscard]] SetupTimeMatrix const& setup_times() const {
        assert(setup_times_.has_value());
        return *setup_times_;
    }

    /// Setup time from operation `from` to `to` on machine `m`.
    /// Returns 0 if no setup times are defined.
    [[nodiscard]] int setup_time(int from, int to, int m) const {
        if (!setup_times_.has_value()) {
            return 0;
        }
        return setup_times_->setup_time(from, to, m);
    }

    /// Whether any machine has calendar restrictions.
    [[nodiscard]] bool has_calendar() const noexcept { return calendar_.has_value(); }

    /// Machine calendar (only valid if has_calendar() is true).
    [[nodiscard]] MachineCalendar const& calendar() const {
        assert(calendar_.has_value());
        return *calendar_;
    }

    /// Processing time for operation o on machine m.
    /// Returns INT_MAX if the operation cannot run on that machine.
    [[nodiscard]] int processing_time(int o, int m) const {
        assert(o >= 0 && o < num_operations_);
        assert(m >= 0 && m < num_machines_);
        return processing_times_[o * num_machines_ + m];
    }

private:
    int num_machines_ = 0;
    int num_jobs_ = 0;
    int num_operations_ = 0;

    std::vector<OperationData> operations_;
    std::vector<JobData> jobs_;
    std::vector<PrecedenceArc> precedences_;

    /// Flat row-major matrix: operation * num_machines_ + machine.
    /// INT_MAX means the operation cannot run on that machine.
    std::vector<int> processing_times_;

    ScheduleObjective objective_ = ScheduleObjective::Makespan;

    /// Sequence-dependent setup times (nullopt if none).
    std::optional<SetupTimeMatrix> setup_times_;

    /// Machine calendars (nullopt if none).
    std::optional<MachineCalendar> calendar_;

    // Construction helper — only Builder can create.
    ScheduleData() = default;
    friend class Builder;
};

}  // namespace coso
