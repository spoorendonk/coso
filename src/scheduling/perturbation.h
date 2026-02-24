#pragma once

#include "scheduling/disjunctive_graph.h"
#include "scheduling/schedule_data.h"

#include <random>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  RandomBlockRemoval
// ---------------------------------------------------------------------------

/// Remove a random block of operations from one or more machine sequences,
/// then re-insert them using a greedy earliest-start heuristic.
///
/// This is a ruin-and-recreate perturbation: it destroys part of the current
/// solution and rebuilds it, enabling the search to escape local optima.
class RandomBlockRemoval {
public:
    /// Parameters controlling the perturbation intensity.
    struct Params {
        int min_block_size = 2;  ///< minimum number of ops to remove
        int max_block_size = 5;  ///< maximum number of ops to remove
    };

    /// Apply the perturbation to the disjunctive graph.
    ///
    /// Removes a random contiguous block of operations from a randomly chosen
    /// machine, then re-inserts each removed operation at the position that
    /// minimises the resulting makespan.
    ///
    /// @returns the new makespan after perturbation.
    static int apply(DisjunctiveGraph& graph, Params const& params,
                     std::mt19937& rng);
};

// ---------------------------------------------------------------------------
//  CriticalPathShake
// ---------------------------------------------------------------------------

/// Randomly perturb operations on or near the critical path.
///
/// Identifies operations on the critical path, then applies random swaps
/// or insertions within their machine sequences.  This focuses perturbation
/// effort on the parts of the schedule that actually constrain the makespan.
class CriticalPathShake {
public:
    /// Parameters controlling the perturbation.
    struct Params {
        int num_perturbations = 3;  ///< number of random moves to attempt
    };

    /// Apply the perturbation.
    ///
    /// For each perturbation step, picks a random critical-path operation
    /// and either swaps it with an adjacent operation on its machine, or
    /// moves it to a random position on its machine.
    ///
    /// @returns the new makespan after perturbation.
    static int apply(DisjunctiveGraph& graph, Params const& params,
                     std::mt19937& rng);
};

// ---------------------------------------------------------------------------
//  MachineReassignment
// ---------------------------------------------------------------------------

/// For FJSP: randomly reassign some operations to different eligible machines.
///
/// Picks a subset of operations that have multiple eligible machines (flexible
/// operations) and reassigns each to a randomly chosen alternative machine.
/// The machine sequences are then rebuilt to accommodate the changes.
class MachineReassignment {
public:
    /// Parameters controlling the perturbation.
    struct Params {
        int num_reassignments = 2;  ///< number of operations to reassign
    };

    /// Apply the perturbation.
    ///
    /// Requires ScheduleData (to know eligible machines) and modifies the
    /// DisjunctiveGraph (machine assignments and sequences).
    ///
    /// @returns the new makespan after perturbation, or -1 if no flexible
    ///          operations exist.
    static int apply(DisjunctiveGraph& graph,
                     ScheduleData const& data,
                     Params const& params,
                     std::mt19937& rng);
};

} // namespace coso
