#include "packing/packing_solution.h"

#include <algorithm>
#include <cassert>

namespace coso {

PackingSolution::PackingSolution(PackingData const& data)
    : data_(&data), unassigned_count_(data.num_items()) {
    // Allocate bin slots: the bin supply is unlimited, so num_items is an
    // upper bound on the number of bins any solution can use.
    num_bins_ = data.num_items();

    bin_items_.resize(num_bins_);
    bin_load_.resize(num_bins_ * data.num_dims(), 0);

    // All items start unassigned.
    item_bin_.assign(data.num_items(), -1);
}

// ---------------------------------------------------------------------------
//  Queries
// ---------------------------------------------------------------------------

bool PackingSolution::item_fits_capacity(int item, int b) const {
    int D = data_->num_dims();
    for (int d = 0; d < D; ++d) {
        if (bin_load_[b * D + d] + data_->item_size(item, d) > data_->bin_capacity(d)) {
            return false;
        }
    }
    return true;
}

bool PackingSolution::has_conflict_in_bin(int item, int b) const {
    auto const& nbrs = data_->conflicts(item);
    if (nbrs.empty()) {
        return false;
    }

    for (int other : bin_items_[b]) {
        if (data_->has_conflict(item, other)) {
            return true;
        }
    }
    return false;
}

bool PackingSolution::item_fits(int item, int b) const {
    return item_fits_capacity(item, b) && !has_conflict_in_bin(item, b);
}

// ---------------------------------------------------------------------------
//  Modification
// ---------------------------------------------------------------------------

void PackingSolution::assign(int item, int bin) {
    assert(item >= 0 && item < data_->num_items());
    assert(bin >= 0 && bin < num_bins_);
    assert(item_bin_[item] == -1);  // must be unassigned

    int D = data_->num_dims();
    bool was_empty = bin_items_[bin].empty();

    // Update load and track capacity violations.
    int old_cap_viol = count_capacity_violations(bin);
    for (int d = 0; d < D; ++d) {
        bin_load_[bin * D + d] += data_->item_size(item, d);
    }
    int new_cap_viol = count_capacity_violations(bin);
    capacity_violations_ += (new_cap_viol - old_cap_viol);

    // Update conflict violations.
    for (int other : bin_items_[bin]) {
        if (data_->has_conflict(item, other)) {
            ++conflict_violations_;
        }
    }

    // Add item to bin.
    bin_items_[bin].push_back(item);
    item_bin_[item] = bin;
    --unassigned_count_;

    if (was_empty) {
        ++bins_used_;
    }
}

void PackingSolution::unassign(int item) {
    assert(item >= 0 && item < data_->num_items());
    int bin = item_bin_[item];
    assert(bin >= 0 && bin < num_bins_);

    int D = data_->num_dims();

    // Remove conflict violations involving this item in its bin.
    for (int other : bin_items_[bin]) {
        if (other != item && data_->has_conflict(item, other)) {
            --conflict_violations_;
        }
    }

    // Update load and capacity violations.
    int old_cap_viol = count_capacity_violations(bin);
    for (int d = 0; d < D; ++d) {
        bin_load_[bin * D + d] -= data_->item_size(item, d);
    }
    int new_cap_viol = count_capacity_violations(bin);
    capacity_violations_ += (new_cap_viol - old_cap_viol);

    // Remove item from bin.
    auto& items = bin_items_[bin];
    items.erase(std::find(items.begin(), items.end(), item));
    item_bin_[item] = -1;
    ++unassigned_count_;

    if (items.empty()) {
        --bins_used_;
    }
}

void PackingSolution::move(int item, int to_bin) {
    assert(item >= 0 && item < data_->num_items());
    assert(to_bin >= 0 && to_bin < num_bins_);
    assert(item_bin_[item] >= 0);  // must be assigned
    assert(item_bin_[item] != to_bin);

    unassign(item);
    assign(item, to_bin);
}

// ---------------------------------------------------------------------------
//  Delta evaluation
// ---------------------------------------------------------------------------

int PackingSolution::assign_cost_delta(int item, int b) const {
    assert(item >= 0 && item < data_->num_items());
    assert(b >= 0 && b < num_bins_);
    assert(item_bin_[item] == -1);

    if (bin_items_[b].empty()) {
        // Opening a new bin.
        return 1;
    }
    // Bin already open, no cost change.
    return 0;
}

int PackingSolution::move_cost_delta(int item, int to_bin) const {
    assert(item >= 0 && item < data_->num_items());
    assert(to_bin >= 0 && to_bin < num_bins_);
    int from_bin = item_bin_[item];
    assert(from_bin >= 0);
    assert(from_bin != to_bin);

    int delta = 0;

    // Cost of opening to_bin if currently empty.
    if (bin_items_[to_bin].empty()) {
        delta += 1;
    }

    // Cost saved if from_bin becomes empty (only item there).
    if (bin_items_[from_bin].size() == 1) {
        delta -= 1;
    }

    return delta;
}

// ---------------------------------------------------------------------------
//  Feasibility
// ---------------------------------------------------------------------------

bool PackingSolution::feasible() const noexcept {
    return capacity_violations_ == 0 && conflict_violations_ == 0 && unassigned_count_ == 0;
}

int PackingSolution::count_capacity_violations(int b) const {
    int D = data_->num_dims();
    int violations = 0;
    for (int d = 0; d < D; ++d) {
        if (bin_load_[b * D + d] > data_->bin_capacity(d)) {
            ++violations;
        }
    }
    return violations;
}

int PackingSolution::count_conflict_violations(int b) const {
    auto const& items = bin_items_[b];
    int violations = 0;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(items.size()); ++j) {
            if (data_->has_conflict(items[i], items[j])) {
                ++violations;
            }
        }
    }
    return violations;
}

}  // namespace coso
