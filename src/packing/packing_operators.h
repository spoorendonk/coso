#pragma once

#include "packing/packing_solution.h"

#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Move descriptors
// ---------------------------------------------------------------------------

/// Describes moving a single item from one bin to another.
struct MoveItem {
    int item    = -1;
    int to_bin  = -1;
    int delta   = 0;   ///< cost delta (negative = improvement)
};

/// Describes swapping two items between different bins.
struct SwapItems {
    int item_a  = -1;  ///< item in bin A
    int item_b  = -1;  ///< item in bin B
    int delta   = 0;
};

/// Describes splitting a bin by moving some items to a new bin.
struct SplitBin {
    int source_bin       = -1;
    int target_bin       = -1;   ///< empty bin to receive items
    std::vector<int> items_to_move;  ///< items moved to target_bin
    int delta            = 0;
};

/// Describes merging all items from one bin into another.
struct MergeBins {
    int source_bin  = -1;  ///< bin that becomes empty after merge
    int target_bin  = -1;  ///< bin that receives all items
    int delta       = 0;
};

// ---------------------------------------------------------------------------
//  Operator functions
// ---------------------------------------------------------------------------

/// Evaluate the cost delta for moving item to to_bin.
/// Returns 0 and sets move.delta; does NOT check feasibility.
[[nodiscard]] MoveItem evaluate_move(PackingSolution const& sol,
                                     int item, int to_bin);

/// Check if a move is feasible (capacity + conflicts in target bin,
/// excluding the item being moved if it was already there).
[[nodiscard]] bool is_feasible(PackingSolution const& sol,
                               MoveItem const& move);

/// Apply a MoveItem to the solution.
void apply(PackingSolution& sol, MoveItem const& move);

/// Enumerate all feasible MoveItem moves.
[[nodiscard]] std::vector<MoveItem> enumerate_moves(
    PackingSolution const& sol);

/// Evaluate the cost delta for swapping two items between bins.
[[nodiscard]] SwapItems evaluate_swap(PackingSolution const& sol,
                                      int item_a, int item_b);

/// Check if a swap is feasible (both items fit in their new bins
/// after the exchange, respecting capacity and conflicts).
[[nodiscard]] bool is_feasible(PackingSolution const& sol,
                               SwapItems const& swap);

/// Apply a SwapItems to the solution.
void apply(PackingSolution& sol, SwapItems const& swap);

/// Enumerate all feasible SwapItems moves.
[[nodiscard]] std::vector<SwapItems> enumerate_swaps(
    PackingSolution const& sol);

/// Evaluate the cost delta for merging source_bin into target_bin.
[[nodiscard]] MergeBins evaluate_merge(PackingSolution const& sol,
                                       int source_bin, int target_bin);

/// Check if a merge is feasible (all source items fit in target bin
/// capacity-wise and conflict-wise).
[[nodiscard]] bool is_feasible(PackingSolution const& sol,
                               MergeBins const& merge);

/// Apply a MergeBins to the solution.
void apply(PackingSolution& sol, MergeBins const& merge);

/// Enumerate all feasible MergeBins moves.
[[nodiscard]] std::vector<MergeBins> enumerate_merges(
    PackingSolution const& sol);

/// Evaluate a split: move items_to_move from source_bin to target_bin.
[[nodiscard]] SplitBin evaluate_split(PackingSolution const& sol,
                                      int source_bin, int target_bin,
                                      std::vector<int> items_to_move);

/// Check if a split is feasible (moved items fit in target bin).
[[nodiscard]] bool is_feasible(PackingSolution const& sol,
                               SplitBin const& split);

/// Apply a SplitBin to the solution.
void apply(PackingSolution& sol, SplitBin const& split);

/// Enumerate split moves for overloaded bins. For each overloaded bin,
/// attempts to split items into the first available empty bin of the
/// same type. Uses a greedy heuristic to decide which items to move.
[[nodiscard]] std::vector<SplitBin> enumerate_splits(
    PackingSolution const& sol);

} // namespace coso
