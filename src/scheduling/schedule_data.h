#pragma once

#include "model/schedule_model.h"

#include <cassert>
#include <climits>
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
///   - Resource capacities and usage matrix (for RCPSP)
///   - Objective type and job parameters
class ScheduleData {
public:
    // -------------------------------------------------------------------
    //  Operation data
    // -------------------------------------------------------------------

    struct OperationData {
        int job           = -1;    ///< owning job index
        int fixed_machine = -1;    ///< -1 = flexible (FJSP)
        int duration      = 0;     ///< fixed duration (when machine is fixed)
        bool optional     = false;
        std::vector<int> eligible_machines;      ///< for FJSP
        std::vector<int> durations_per_machine;  ///< parallel to eligible_machines
    };

    // -------------------------------------------------------------------
    //  Job data
    // -------------------------------------------------------------------

    struct JobData {
        std::string name;
        int release_time  = 0;
        int due_date      = INT_MAX;
        int weight        = 1;
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

        /// Add a renewable resource with the given capacity.
        int add_resource(int capacity);

        /// Set resource usage for an operation.
        void set_resource_usage(int operation, int resource, int amount);

        /// Add a precedence constraint (beyond default intra-job ordering).
        void add_precedence(int op_before, int op_after);

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

        /// Resource capacities.
        std::vector<int> resource_capacities_;

        /// Per-operation resource usage: resource_usage_[op][res] = amount.
        std::vector<std::vector<int>> resource_usage_;

        /// Extra precedence constraints (beyond intra-job).
        struct Prec { int before; int after; };
        std::vector<Prec> extra_precedences_;

        ScheduleObjective objective_ = ScheduleObjective::Makespan;
    };

    // -------------------------------------------------------------------
    //  Accessors (all const — ScheduleData is immutable after construction)
    // -------------------------------------------------------------------

    [[nodiscard]] int num_machines()   const noexcept { return num_machines_; }
    [[nodiscard]] int num_jobs()       const noexcept { return num_jobs_; }
    [[nodiscard]] int num_operations() const noexcept { return num_operations_; }
    [[nodiscard]] int num_resources()  const noexcept { return num_resources_; }

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

    /// Machine name for machine index m (0-based).
    [[nodiscard]] std::string const& machine_name(int m) const {
        assert(m >= 0 && m < num_machines_);
        return machine_names_[m];
    }

    /// All precedence arcs (intra-job + extra).
    [[nodiscard]] std::span<PrecedenceArc const> precedences() const noexcept {
        return precedences_;
    }

    /// Resource capacity for resource index r.
    [[nodiscard]] int resource_capacity(int r) const {
        assert(r >= 0 && r < num_resources_);
        return resource_capacities_[r];
    }

    /// Resource usage for operation o, resource r.
    [[nodiscard]] int resource_usage(int o, int r) const {
        assert(o >= 0 && o < num_operations_);
        assert(r >= 0 && r < num_resources_);
        return resource_usage_[o * num_resources_ + r];
    }

    /// The scheduling objective.
    [[nodiscard]] ScheduleObjective objective() const noexcept { return objective_; }

    /// Processing time for operation o on machine m.
    /// Returns INT_MAX if the operation cannot run on that machine.
    [[nodiscard]] int processing_time(int o, int m) const {
        assert(o >= 0 && o < num_operations_);
        assert(m >= 0 && m < num_machines_);
        return processing_times_[o * num_machines_ + m];
    }

private:
    int num_machines_   = 0;
    int num_jobs_       = 0;
    int num_operations_ = 0;
    int num_resources_  = 0;

    std::vector<OperationData>  operations_;
    std::vector<JobData>        jobs_;
    std::vector<std::string>    machine_names_;
    std::vector<PrecedenceArc>  precedences_;
    std::vector<int>            resource_capacities_;

    /// Flat row-major matrix: operation * num_machines_ + machine.
    /// INT_MAX means the operation cannot run on that machine.
    std::vector<int> processing_times_;

    /// Flat row-major matrix: operation * num_resources_ + resource.
    std::vector<int> resource_usage_;

    ScheduleObjective objective_ = ScheduleObjective::Makespan;

    // Construction helper — only Builder can create.
    ScheduleData() = default;
    friend class Builder;
};

} // namespace coso
