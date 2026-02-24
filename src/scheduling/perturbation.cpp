#include "scheduling/perturbation.h"

#include <algorithm>
#include <cassert>
#include <climits>

namespace coso {

// ===========================================================================
//  RandomBlockRemoval
// ===========================================================================

int RandomBlockRemoval::apply(DisjunctiveGraph& graph, Params const& params,
                              std::mt19937& rng)
{
    // Pick a random machine that has enough operations.
    std::vector<int> candidates;
    for (int m = 0; m < graph.num_machines(); ++m) {
        if (static_cast<int>(graph.machine_sequence(m).size())
            >= params.min_block_size) {
            candidates.push_back(m);
        }
    }
    if (candidates.empty())
        return graph.critical_path();

    int machine = candidates[std::uniform_int_distribution<int>(
        0, static_cast<int>(candidates.size()) - 1)(rng)];

    auto seq = graph.machine_sequence(machine);
    int n = static_cast<int>(seq.size());

    // Determine block size.
    int max_bs = std::min(params.max_block_size, n);
    int min_bs = std::min(params.min_block_size, max_bs);
    int block_size = std::uniform_int_distribution<int>(min_bs, max_bs)(rng);

    // Pick random start position for the block.
    int start = std::uniform_int_distribution<int>(0, n - block_size)(rng);

    // Extract the block.
    std::vector<int> removed(seq.begin() + start,
                             seq.begin() + start + block_size);

    // Remove the block from the sequence.
    seq.erase(seq.begin() + start, seq.begin() + start + block_size);

    // Re-insert each removed operation at the best position (greedy).
    for (int op : removed) {
        int best_pos = 0;
        int best_ms = INT_MAX;

        // Try each insertion position.
        for (int pos = 0; pos <= static_cast<int>(seq.size()); ++pos) {
            seq.insert(seq.begin() + pos, op);
            graph.set_sequence(machine, seq);
            int ms = graph.critical_path();
            if (ms > 0 && ms < best_ms) {
                best_ms = ms;
                best_pos = pos;
            }
            seq.erase(seq.begin() + pos);
        }

        seq.insert(seq.begin() + best_pos, op);
    }

    graph.set_sequence(machine, seq);
    return graph.critical_path();
}

// ===========================================================================
//  CriticalPathShake
// ===========================================================================

int CriticalPathShake::apply(DisjunctiveGraph& graph, Params const& params,
                             std::mt19937& rng)
{
    for (int step = 0; step < params.num_perturbations; ++step) {
        auto crit_ops = graph.critical_path_ops();
        if (crit_ops.empty())
            break;

        // Pick a random critical-path operation.
        int idx = std::uniform_int_distribution<int>(
            0, static_cast<int>(crit_ops.size()) - 1)(rng);
        int op = crit_ops[idx];

        int machine = graph.operation(op).machine;
        if (machine < 0)
            continue;

        auto seq = graph.machine_sequence(machine);
        int n = static_cast<int>(seq.size());
        if (n < 2)
            continue;

        // Find the position of op in the machine sequence.
        int pos = -1;
        for (int i = 0; i < n; ++i) {
            if (seq[i] == op) {
                pos = i;
                break;
            }
        }
        if (pos < 0)
            continue;

        // Save the current sequence in case the move creates a cycle.
        auto saved_seq = seq;

        // Decide: swap with adjacent (coin flip) or move to random position.
        bool do_swap = (n >= 2)
            && std::uniform_int_distribution<int>(0, 1)(rng) == 0;

        if (do_swap) {
            // Swap with a random adjacent operation.
            int adj = (pos == 0) ? 1
                    : (pos == n - 1) ? pos - 1
                    : (std::uniform_int_distribution<int>(0, 1)(rng) == 0
                           ? pos - 1
                           : pos + 1);
            std::swap(seq[pos], seq[adj]);
        } else {
            // Move to a random different position.
            int target = pos;
            while (target == pos)
                target = std::uniform_int_distribution<int>(0, n - 1)(rng);

            int moved_op = seq[pos];
            seq.erase(seq.begin() + pos);
            // Adjust target index after removal.
            if (target > pos)
                --target;
            seq.insert(seq.begin() + target, moved_op);
        }

        graph.set_sequence(machine, seq);

        // Check for cycles: if makespan is 0 with non-trivial operations,
        // that indicates a cycle -- revert.
        int ms = graph.critical_path();
        bool has_positive_duration = false;
        for (int i = 0; i < graph.num_operations(); ++i) {
            if (graph.operation(i).duration > 0) {
                has_positive_duration = true;
                break;
            }
        }
        if (ms == 0 && has_positive_duration) {
            // Cycle detected, revert this step.
            graph.set_sequence(machine, saved_seq);
        }
    }

    return graph.critical_path();
}

// ===========================================================================
//  MachineReassignment
// ===========================================================================

int MachineReassignment::apply(DisjunctiveGraph& graph,
                               ScheduleData const& data,
                               Params const& params,
                               std::mt19937& rng)
{
    // Find all flexible operations: those with more than one eligible machine.
    std::vector<int> flexible_ops;
    for (int op = 0; op < data.num_operations(); ++op) {
        auto const& opdata = data.operation(op);
        if (static_cast<int>(opdata.eligible_machines.size()) > 1)
            flexible_ops.push_back(op);
    }

    if (flexible_ops.empty())
        return -1;

    int num = std::min(params.num_reassignments,
                       static_cast<int>(flexible_ops.size()));

    // Shuffle and pick the first num operations.
    std::shuffle(flexible_ops.begin(), flexible_ops.end(), rng);

    for (int i = 0; i < num; ++i) {
        int op = flexible_ops[i];
        auto const& opdata = data.operation(op);
        int current_machine = graph.operation(op).machine;

        // Choose a different eligible machine.
        std::vector<int> alternatives;
        for (size_t j = 0; j < opdata.eligible_machines.size(); ++j) {
            int m = opdata.eligible_machines[j];
            if (m != current_machine)
                alternatives.push_back(static_cast<int>(j));
        }
        if (alternatives.empty())
            continue;

        int alt_idx = alternatives[std::uniform_int_distribution<int>(
            0, static_cast<int>(alternatives.size()) - 1)(rng)];
        int new_machine = opdata.eligible_machines[alt_idx];
        int new_duration = opdata.durations_per_machine[alt_idx];

        // Remove op from its current machine sequence.
        auto old_seq = graph.machine_sequence(current_machine);
        auto it = std::find(old_seq.begin(), old_seq.end(), op);
        if (it != old_seq.end()) {
            old_seq.erase(it);
            graph.set_sequence(current_machine, old_seq);
        }

        // Update the operation's machine and duration.
        graph.set_processing_time(op, new_machine, new_duration);

        // Add op to the new machine's sequence at the best position.
        auto new_seq = graph.machine_sequence(new_machine);
        int best_pos = static_cast<int>(new_seq.size());  // default: append
        int best_ms = INT_MAX;

        for (int pos = 0; pos <= static_cast<int>(new_seq.size()); ++pos) {
            new_seq.insert(new_seq.begin() + pos, op);
            graph.set_sequence(new_machine, new_seq);
            int ms = graph.critical_path();
            if (ms > 0 && ms < best_ms) {
                best_ms = ms;
                best_pos = pos;
            }
            new_seq.erase(new_seq.begin() + pos);
        }

        new_seq.insert(new_seq.begin() + best_pos, op);
        graph.set_sequence(new_machine, new_seq);
    }

    return graph.critical_path();
}

} // namespace coso
