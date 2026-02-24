#include "packing/packing_operators.h"

#include <algorithm>
#include <cassert>

namespace coso {

// ---------------------------------------------------------------------------
//  MoveItem
// ---------------------------------------------------------------------------

MoveItem evaluate_move(PackingSolution const& sol, int item, int to_bin)
{
    assert(item >= 0 && item < sol.data().num_items());
    assert(to_bin >= 0 && to_bin < sol.num_bins());
    assert(sol.item_bin(item) >= 0);       // must be assigned
    assert(sol.item_bin(item) != to_bin);   // must be a different bin

    return MoveItem{
        .item   = item,
        .to_bin = to_bin,
        .delta  = sol.move_cost_delta(item, to_bin),
    };
}

bool is_feasible(PackingSolution const& sol, MoveItem const& move)
{
    // Check capacity: does item fit in target bin?
    int const D  = sol.data().num_dims();
    int const bt = sol.bin_type(move.to_bin);
    for (int d = 0; d < D; ++d) {
        if (sol.bin_load(move.to_bin, d) + sol.data().item_size(move.item, d)
            > sol.data().bin_capacity(bt, d))
            return false;
    }

    // Check conflicts: does item conflict with any item already in target?
    // (The item is currently in its source bin, not in target, so no need
    // to exclude it from the conflict check.)
    if (sol.has_conflict_in_bin(move.item, move.to_bin))
        return false;

    return true;
}

void apply(PackingSolution& sol, MoveItem const& move)
{
    sol.move(move.item, move.to_bin);
}

std::vector<MoveItem> enumerate_moves(PackingSolution const& sol)
{
    std::vector<MoveItem> moves;
    int const N = sol.data().num_items();
    int const B = sol.num_bins();

    for (int i = 0; i < N; ++i) {
        int from = sol.item_bin(i);
        if (from < 0) continue;  // unassigned

        for (int b = 0; b < B; ++b) {
            if (b == from) continue;

            // Only consider non-empty bins or the first empty bin of each type
            // to avoid symmetric moves to equivalent empty bins.
            if (sol.bin_items(b).empty()) {
                // Skip if there is an earlier empty bin of the same type.
                bool first_empty = true;
                for (int bb = 0; bb < b; ++bb) {
                    if (sol.bin_items(bb).empty()
                        && sol.bin_type(bb) == sol.bin_type(b)) {
                        first_empty = false;
                        break;
                    }
                }
                if (!first_empty) continue;
            }

            auto move = evaluate_move(sol, i, b);
            if (is_feasible(sol, move)) {
                moves.push_back(move);
            }
        }
    }
    return moves;
}

// ---------------------------------------------------------------------------
//  SwapItems
// ---------------------------------------------------------------------------

SwapItems evaluate_swap(PackingSolution const& sol, int item_a, int item_b)
{
    assert(item_a >= 0 && item_a < sol.data().num_items());
    assert(item_b >= 0 && item_b < sol.data().num_items());
    assert(item_a != item_b);

    int bin_a = sol.item_bin(item_a);
    int bin_b = sol.item_bin(item_b);
    assert(bin_a >= 0 && bin_b >= 0);
    assert(bin_a != bin_b);

    // Swap cost delta: neither bin opens or closes (both keep at least one
    // item, the swapped one), so delta is always 0.
    // Exception: if one bin had exactly one item AND the other bin had exactly
    // one item, then both bins stay open (1 item each after swap). So still 0.
    return SwapItems{
        .item_a = item_a,
        .item_b = item_b,
        .delta  = 0,
    };
}

bool is_feasible(PackingSolution const& sol, SwapItems const& swap)
{
    int const D     = sol.data().num_dims();
    int const bin_a = sol.item_bin(swap.item_a);
    int const bin_b = sol.item_bin(swap.item_b);
    int const bt_a  = sol.bin_type(bin_a);
    int const bt_b  = sol.bin_type(bin_b);

    // Check: item_b fits in bin_a after removing item_a.
    // New load in bin_a = current_load - size(item_a) + size(item_b).
    for (int d = 0; d < D; ++d) {
        int new_load = sol.bin_load(bin_a, d)
                     - sol.data().item_size(swap.item_a, d)
                     + sol.data().item_size(swap.item_b, d);
        if (new_load > sol.data().bin_capacity(bt_a, d))
            return false;
    }

    // Check: item_a fits in bin_b after removing item_b.
    for (int d = 0; d < D; ++d) {
        int new_load = sol.bin_load(bin_b, d)
                     - sol.data().item_size(swap.item_b, d)
                     + sol.data().item_size(swap.item_a, d);
        if (new_load > sol.data().bin_capacity(bt_b, d))
            return false;
    }

    // Check conflicts: item_b in bin_a (excluding item_a which will leave).
    for (int other : sol.bin_items(bin_a)) {
        if (other == swap.item_a) continue;
        if (sol.data().has_conflict(swap.item_b, other))
            return false;
    }

    // Check conflicts: item_a in bin_b (excluding item_b which will leave).
    for (int other : sol.bin_items(bin_b)) {
        if (other == swap.item_b) continue;
        if (sol.data().has_conflict(swap.item_a, other))
            return false;
    }

    // Also check conflict between item_a and item_b is irrelevant since they
    // end up in different bins.
    return true;
}

void apply(PackingSolution& sol, SwapItems const& swap)
{
    int bin_a = sol.item_bin(swap.item_a);
    int bin_b = sol.item_bin(swap.item_b);

    // Unassign both, then assign to swapped bins.
    sol.unassign(swap.item_a);
    sol.unassign(swap.item_b);
    sol.assign(swap.item_a, bin_b);
    sol.assign(swap.item_b, bin_a);
}

std::vector<SwapItems> enumerate_swaps(PackingSolution const& sol)
{
    std::vector<SwapItems> swaps;
    int const N = sol.data().num_items();

    for (int a = 0; a < N; ++a) {
        int bin_a = sol.item_bin(a);
        if (bin_a < 0) continue;

        for (int b = a + 1; b < N; ++b) {
            int bin_b = sol.item_bin(b);
            if (bin_b < 0 || bin_b == bin_a) continue;

            auto swap = evaluate_swap(sol, a, b);
            if (is_feasible(sol, swap)) {
                swaps.push_back(swap);
            }
        }
    }
    return swaps;
}

// ---------------------------------------------------------------------------
//  MergeBins
// ---------------------------------------------------------------------------

MergeBins evaluate_merge(PackingSolution const& sol,
                         int source_bin, int target_bin)
{
    assert(source_bin >= 0 && source_bin < sol.num_bins());
    assert(target_bin >= 0 && target_bin < sol.num_bins());
    assert(source_bin != target_bin);
    assert(!sol.bin_items(source_bin).empty());

    // Merging source into target: source bin closes.
    int delta = -sol.data().bin_cost(sol.bin_type(source_bin));

    // If target is currently empty, it opens.
    if (sol.bin_items(target_bin).empty()) {
        delta += sol.data().bin_cost(sol.bin_type(target_bin));
    }

    return MergeBins{
        .source_bin = source_bin,
        .target_bin = target_bin,
        .delta      = delta,
    };
}

bool is_feasible(PackingSolution const& sol, MergeBins const& merge)
{
    int const D  = sol.data().num_dims();
    int const bt = sol.bin_type(merge.target_bin);

    // Check capacity: can target bin hold all source items too?
    for (int d = 0; d < D; ++d) {
        int combined = sol.bin_load(merge.target_bin, d)
                     + sol.bin_load(merge.source_bin, d);
        if (combined > sol.data().bin_capacity(bt, d))
            return false;
    }

    // Check conflicts: no source item conflicts with any target item.
    for (int si : sol.bin_items(merge.source_bin)) {
        for (int ti : sol.bin_items(merge.target_bin)) {
            if (sol.data().has_conflict(si, ti))
                return false;
        }
        // Also check conflicts among source items moving together -- but
        // they are already in the same bin so no new conflicts arise.
    }

    return true;
}

void apply(PackingSolution& sol, MergeBins const& merge)
{
    // Copy the items list since we will modify it during iteration.
    auto items = sol.bin_items(merge.source_bin);
    for (int item : items) {
        sol.move(item, merge.target_bin);
    }
}

std::vector<MergeBins> enumerate_merges(PackingSolution const& sol)
{
    std::vector<MergeBins> merges;
    int const B = sol.num_bins();

    // Collect non-empty bins.
    std::vector<int> used;
    for (int b = 0; b < B; ++b) {
        if (!sol.bin_items(b).empty())
            used.push_back(b);
    }

    for (int i = 0; i < static_cast<int>(used.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(used.size()); ++j) {
            // Try merging i into j.
            auto merge_ij = evaluate_merge(sol, used[i], used[j]);
            if (is_feasible(sol, merge_ij))
                merges.push_back(merge_ij);

            // Try merging j into i.
            auto merge_ji = evaluate_merge(sol, used[j], used[i]);
            if (is_feasible(sol, merge_ji))
                merges.push_back(merge_ji);
        }
    }

    return merges;
}

// ---------------------------------------------------------------------------
//  SplitBin
// ---------------------------------------------------------------------------

SplitBin evaluate_split(PackingSolution const& sol,
                        int source_bin, int target_bin,
                        std::vector<int> items_to_move)
{
    assert(source_bin >= 0 && source_bin < sol.num_bins());
    assert(target_bin >= 0 && target_bin < sol.num_bins());
    assert(source_bin != target_bin);
    assert(!items_to_move.empty());

    // Source bin stays open (some items remain), target bin opens.
    int delta = 0;
    if (sol.bin_items(target_bin).empty()) {
        delta += sol.data().bin_cost(sol.bin_type(target_bin));
    }

    // If we are moving ALL items from source, source closes.
    if (static_cast<int>(items_to_move.size())
        == static_cast<int>(sol.bin_items(source_bin).size())) {
        delta -= sol.data().bin_cost(sol.bin_type(source_bin));
    }

    return SplitBin{
        .source_bin     = source_bin,
        .target_bin     = target_bin,
        .items_to_move  = std::move(items_to_move),
        .delta          = delta,
    };
}

bool is_feasible(PackingSolution const& sol, SplitBin const& split)
{
    int const D  = sol.data().num_dims();
    int const bt = sol.bin_type(split.target_bin);

    // Check capacity: do moved items fit in target bin?
    for (int d = 0; d < D; ++d) {
        int load = sol.bin_load(split.target_bin, d);
        for (int item : split.items_to_move) {
            load += sol.data().item_size(item, d);
        }
        if (load > sol.data().bin_capacity(bt, d))
            return false;
    }

    // Check conflicts among moved items + existing target items.
    for (int item : split.items_to_move) {
        // Conflict with existing target bin items.
        for (int ti : sol.bin_items(split.target_bin)) {
            if (sol.data().has_conflict(item, ti))
                return false;
        }
    }

    // Check conflicts among the moved items themselves.
    for (int i = 0; i < static_cast<int>(split.items_to_move.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(split.items_to_move.size()); ++j) {
            if (sol.data().has_conflict(split.items_to_move[i],
                                        split.items_to_move[j]))
                return false;
        }
    }

    return true;
}

void apply(PackingSolution& sol, SplitBin const& split)
{
    for (int item : split.items_to_move) {
        sol.move(item, split.target_bin);
    }
}

std::vector<SplitBin> enumerate_splits(PackingSolution const& sol)
{
    std::vector<SplitBin> splits;
    int const B = sol.num_bins();
    int const D = sol.data().num_dims();

    for (int b = 0; b < B; ++b) {
        auto const& items = sol.bin_items(b);
        if (items.size() < 2) continue;  // nothing to split

        // Check if bin is overloaded in any dimension.
        bool overloaded = false;
        int bt = sol.bin_type(b);
        for (int d = 0; d < D; ++d) {
            if (sol.bin_load(b, d) > sol.data().bin_capacity(bt, d)) {
                overloaded = true;
                break;
            }
        }
        if (!overloaded) continue;

        // Find the first empty bin of the same type.
        int target = -1;
        for (int tb = 0; tb < B; ++tb) {
            if (sol.bin_items(tb).empty() && sol.bin_type(tb) == bt) {
                target = tb;
                break;
            }
        }
        if (target < 0) continue;  // no empty bin available

        // Greedy split: sort items by largest size (dim 0), move items to
        // target until target is roughly half full or source becomes feasible.
        auto sorted_items = items;
        std::sort(sorted_items.begin(), sorted_items.end(),
                  [&](int a, int ab) {
                      return sol.data().item_size(a, 0)
                           > sol.data().item_size(ab, 0);
                  });

        std::vector<int> to_move;
        std::vector<int> target_load(D, 0);
        std::vector<int> remaining_load(D);
        for (int d = 0; d < D; ++d)
            remaining_load[d] = sol.bin_load(b, d);

        for (int item : sorted_items) {
            // Check if moving this item to target would exceed target capacity.
            bool fits = true;
            for (int d = 0; d < D; ++d) {
                if (target_load[d] + sol.data().item_size(item, d)
                    > sol.data().bin_capacity(bt, d)) {
                    fits = false;
                    break;
                }
            }
            if (!fits) continue;

            to_move.push_back(item);
            for (int d = 0; d < D; ++d) {
                int sz = sol.data().item_size(item, d);
                target_load[d]    += sz;
                remaining_load[d] -= sz;
            }

            // Check if source is now feasible.
            bool source_ok = true;
            for (int d = 0; d < D; ++d) {
                if (remaining_load[d] > sol.data().bin_capacity(bt, d)) {
                    source_ok = false;
                    break;
                }
            }
            if (source_ok) break;
        }

        if (!to_move.empty()) {
            auto split = evaluate_split(sol, b, target, std::move(to_move));
            if (is_feasible(sol, split))
                splits.push_back(std::move(split));
        }
    }

    return splits;
}

} // namespace coso
