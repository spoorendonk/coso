#pragma once

#include <cassert>
#include <climits>
#include <vector>

namespace coso {

/// Disjunctive graph for job-shop scheduling problems.
///
/// Represents operations as nodes with:
///   - Conjunctive arcs: fixed precedence within each job.
///   - Disjunctive arcs: machine sequencing (mutable, set during search).
///   - Source/sink dummy nodes for start/end.
///
/// Supports forward/backward passes for earliest/latest start times,
/// critical path computation, and incremental updates when machine
/// sequences change.
class DisjunctiveGraph {
public:
    /// Operation descriptor: which job it belongs to, machine, duration.
    struct Operation {
        int job;       ///< job index
        int machine;   ///< assigned machine (-1 if unassigned / flexible)
        int duration;  ///< processing time on the assigned machine
    };

    // -------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------

    /// Construct an empty disjunctive graph.
    ///
    /// @param num_jobs      Number of jobs.
    /// @param num_machines  Number of machines.
    DisjunctiveGraph(int num_jobs, int num_machines);

    /// Add an operation for a job.  Operations within a job are ordered by
    /// insertion order (conjunctive arcs are created automatically).
    ///
    /// @returns operation index (0-based, globally unique).
    int add_operation(int job, int machine, int duration);

    /// Change the processing time for an existing operation.
    void set_processing_time(int op, int machine, int duration);

    // -------------------------------------------------------------------
    //  Machine sequencing (disjunctive arcs)
    // -------------------------------------------------------------------

    /// Fix the processing order on a machine.
    ///
    /// @param machine  Machine index.
    /// @param ops      Operation indices in desired order.
    ///
    /// This replaces the current sequence for the machine and marks the
    /// graph dirty so that the next query recomputes times.
    void set_sequence(int machine, std::vector<int> const& ops);

    // -------------------------------------------------------------------
    //  Queries (recompute on demand)
    // -------------------------------------------------------------------

    /// Earliest start time for an operation (forward pass).
    [[nodiscard]] int start_time(int op);

    /// Earliest completion time for an operation.
    [[nodiscard]] int completion_time(int op);

    /// Latest start time for an operation (backward pass).
    [[nodiscard]] int latest_start_time(int op);

    /// Makespan = critical path length = longest path from source to sink.
    [[nodiscard]] int critical_path();

    /// Operations on the critical path (source and sink excluded).
    [[nodiscard]] std::vector<int> critical_path_ops();

    /// A valid topological ordering of all operations (source/sink excluded).
    [[nodiscard]] std::vector<int> topological_order();

    /// Longest path between two operations (returns -1 if unreachable).
    [[nodiscard]] int longest_path(int from, int to);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    [[nodiscard]] int num_jobs() const noexcept { return num_jobs_; }
    [[nodiscard]] int num_machines() const noexcept { return num_machines_; }
    [[nodiscard]] int num_operations() const noexcept {
        return static_cast<int>(ops_.size());
    }

    /// Source node index (virtual, = num_operations).
    [[nodiscard]] int source() const noexcept {
        return static_cast<int>(ops_.size());
    }

    /// Sink node index (virtual, = num_operations + 1).
    [[nodiscard]] int sink() const noexcept {
        return static_cast<int>(ops_.size()) + 1;
    }

    /// Access operation data.
    [[nodiscard]] Operation const& operation(int op) const {
        assert(op >= 0 && op < num_operations());
        return ops_[op];
    }

    /// Operations belonging to a job (in precedence order).
    [[nodiscard]] std::vector<int> const& job_operations(int job) const {
        assert(job >= 0 && job < num_jobs_);
        return job_ops_[job];
    }

    /// Current sequence for a machine (empty if not yet set).
    [[nodiscard]] std::vector<int> const& machine_sequence(int machine) const {
        assert(machine >= 0 && machine < num_machines_);
        return machine_seq_[machine];
    }

private:
    /// Rebuild the adjacency list and recompute times.
    void recompute();

    /// Forward pass: compute earliest start/completion times.
    void forward_pass(std::vector<int> const& topo);

    /// Backward pass: compute latest start times.
    void backward_pass(std::vector<int> const& topo);

    /// Topological sort of the full graph (including source/sink).
    [[nodiscard]] std::vector<int> topo_sort() const;

    int num_jobs_;
    int num_machines_;

    std::vector<Operation> ops_;
    std::vector<std::vector<int>> job_ops_;      ///< ops per job
    std::vector<std::vector<int>> machine_seq_;   ///< current sequence per machine

    // Adjacency list: successors of each node.
    // Nodes 0..num_ops-1 are operations, num_ops = source, num_ops+1 = sink.
    std::vector<std::vector<int>> adj_;

    // Computed times (invalidated on mutation).
    std::vector<int> earliest_start_;   ///< forward pass
    std::vector<int> earliest_finish_;
    std::vector<int> latest_start_;     ///< backward pass
    bool dirty_ = true;
};

} // namespace coso
