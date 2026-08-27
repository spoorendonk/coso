#pragma once

#include "packing/packing_data.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// A complete solution to a bin packing problem.
///
/// Tracks item-to-bin assignments, per-bin load vectors, and bin usage.
/// Supports incremental assign/unassign/move operations with O(D) cost
/// (where D is the number of dimensions), and O(1) cost/feasibility queries.
///
/// Bin slots are pre-allocated: one slot per bin type instance. For bin type b
/// with count limit C, slots run from offset[b] to offset[b]+C-1. When count
/// is unlimited (0), a generous upper bound (num_items) is used per type.
class PackingSolution {
public:
    /// Construct an empty solution (all items unassigned).
    explicit PackingSolution(PackingData const& data);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The problem data this solution belongs to.
    [[nodiscard]] PackingData const& data() const noexcept { return *data_; }

    /// Total number of bin slots.
    [[nodiscard]] int num_bins() const noexcept { return num_bins_; }

    /// Number of non-empty bins.
    [[nodiscard]] int num_bins_used() const noexcept { return bins_used_; }

    /// Bin type index for bin slot b.
    [[nodiscard]] int bin_type(int b) const noexcept {
        assert(b >= 0 && b < num_bins_);
        return bin_type_[b];
    }

    /// Which bin an item is assigned to, or -1 if unassigned.
    [[nodiscard]] int item_bin(int item) const noexcept {
        assert(item >= 0 && item < data_->num_items());
        return item_bin_[item];
    }

    /// Items currently in bin b.
    [[nodiscard]] std::vector<int> const& bin_items(int b) const noexcept {
        assert(b >= 0 && b < num_bins_);
        return bin_items_[b];
    }

    /// Current load of bin b in dimension d.
    [[nodiscard]] int bin_load(int b, int d) const noexcept {
        assert(b >= 0 && b < num_bins_ && d >= 0 && d < data_->num_dims());
        return bin_load_[b * data_->num_dims() + d];
    }

    /// Remaining capacity of bin b in dimension d.
    [[nodiscard]] int bin_remaining(int b, int d) const noexcept {
        int bt = bin_type_[b];
        return data_->bin_capacity(bt, d) - bin_load(b, d);
    }

    /// Whether item fits in bin b (all dimensions and no conflicts).
    [[nodiscard]] bool item_fits(int item, int b) const;

    /// Whether item fits capacity of bin b (ignoring conflicts).
    [[nodiscard]] bool item_fits_capacity(int item, int b) const;

    /// Whether assigning item to bin b would violate a conflict.
    [[nodiscard]] bool has_conflict_in_bin(int item, int b) const;

    // -------------------------------------------------------------------
    //  Modification
    // -------------------------------------------------------------------

    /// Assign an unassigned item to a bin.
    void assign(int item, int bin);

    /// Unassign an item from its current bin.
    void unassign(int item);

    /// Move an item from its current bin to a different bin.
    void move(int item, int to_bin);

    // -------------------------------------------------------------------
    //  Objective
    // -------------------------------------------------------------------

    /// Total cost: sum of bin_cost for each non-empty bin.
    [[nodiscard]] int cost() const noexcept { return cost_; }

    /// Delta cost if we were to assign an unassigned item to bin b.
    /// Returns the change in cost (positive = increase).
    [[nodiscard]] int assign_cost_delta(int item, int b) const;

    /// Delta cost if we were to move item from its current bin to to_bin.
    [[nodiscard]] int move_cost_delta(int item, int to_bin) const;

    // -------------------------------------------------------------------
    //  Feasibility
    // -------------------------------------------------------------------

    /// True if no capacity violations and no conflict violations.
    [[nodiscard]] bool feasible() const noexcept;

    /// Number of capacity violations (bins where load > capacity in any dim).
    [[nodiscard]] int num_capacity_violations() const noexcept { return capacity_violations_; }

    /// Number of conflict violations (pairs of conflicting items in same bin).
    [[nodiscard]] int num_conflict_violations() const noexcept { return conflict_violations_; }

    /// True if all items are assigned.
    [[nodiscard]] bool all_assigned() const noexcept { return unassigned_count_ == 0; }

    /// Number of unassigned items.
    [[nodiscard]] int num_unassigned() const noexcept { return unassigned_count_; }

private:
    PackingData const* data_ = nullptr;

    int num_bins_ = 0;

    // bin_type_[b] = type index for bin slot b.
    std::vector<int> bin_type_;

    // item_bin_[i] = bin slot index, or -1 if unassigned.
    std::vector<int> item_bin_;

    // bin_items_[b] = list of item indices in bin b.
    std::vector<std::vector<int>> bin_items_;

    // Flat load array: bin_load_[b * num_dims + d].
    std::vector<int> bin_load_;

    // Cached aggregates.
    int bins_used_ = 0;
    int cost_ = 0;
    int capacity_violations_ = 0;
    int conflict_violations_ = 0;
    int unassigned_count_ = 0;

    /// Count capacity violations for bin b.
    [[nodiscard]] int count_capacity_violations(int b) const;

    /// Count conflict violations for bin b.
    [[nodiscard]] int count_conflict_violations(int b) const;
};

}  // namespace coso
