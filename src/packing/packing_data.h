#pragma once

#include "model/packing_model.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

namespace coso {

/// Compiled, immutable representation of a bin packing instance.
///
/// Built once from a PackingModel. Provides efficient, cache-friendly access
/// patterns for bin packing algorithms:
///   - Flat contiguous arrays for bin capacities and item sizes
///   - Adjacency-list conflict graph
///   - Precomputed lower bounds (continuous, L2)
class PackingData {
public:
    // -------------------------------------------------------------------
    //  Builder — the only way to construct a PackingData
    // -------------------------------------------------------------------

    /// Build a PackingData from a PackingModel.
    static PackingData build(PackingModel const& model) {
        PackingData data;

        data.num_bin_types_ = model.num_bin_types();
        data.num_items_ = model.num_items();
        data.num_dims_ = model.num_dimensions();

        // Copy bin type data into flat arrays.
        data.bin_capacities_.resize(data.num_bin_types_ * data.num_dims_);
        data.bin_costs_.resize(data.num_bin_types_);
        data.bin_counts_.resize(data.num_bin_types_);

        for (int b = 0; b < data.num_bin_types_; ++b) {
            auto const& bt = model.bin_type(b);
            for (int d = 0; d < data.num_dims_; ++d) {
                data.bin_capacities_[b * data.num_dims_ + d] = bt.capacity[d];
            }
            data.bin_costs_[b] = bt.cost;
            data.bin_counts_[b] = bt.count;
        }

        // Copy item sizes into flat array.
        data.item_sizes_.resize(data.num_items_ * data.num_dims_);
        for (int i = 0; i < data.num_items_; ++i) {
            auto const& it = model.item(i);
            for (int d = 0; d < data.num_dims_; ++d) {
                data.item_sizes_[i * data.num_dims_ + d] = it.size[d];
            }
        }

        // Build adjacency-list conflict graph.
        data.conflict_adj_.resize(data.num_items_);
        for (auto const& [a, b] : model.conflicts()) {
            data.conflict_adj_[a].push_back(b);
            data.conflict_adj_[b].push_back(a);
        }

        // Precompute lower bounds.
        data.compute_lower_bounds_();

        return data;
    }

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    [[nodiscard]] int num_bin_types() const noexcept { return num_bin_types_; }
    [[nodiscard]] int num_items() const noexcept { return num_items_; }
    [[nodiscard]] int num_dims() const noexcept { return num_dims_; }

    /// Capacity of bin type b in dimension d.
    [[nodiscard]] int bin_capacity(int b, int d) const {
        assert(b >= 0 && b < num_bin_types_ && d >= 0 && d < num_dims_);
        return bin_capacities_[b * num_dims_ + d];
    }

    /// Cost of bin type b.
    [[nodiscard]] int bin_cost(int b) const {
        assert(b >= 0 && b < num_bin_types_);
        return bin_costs_[b];
    }

    /// Count limit of bin type b (0 = unlimited).
    [[nodiscard]] int bin_count(int b) const {
        assert(b >= 0 && b < num_bin_types_);
        return bin_counts_[b];
    }

    /// Size of item i in dimension d.
    [[nodiscard]] int item_size(int i, int d) const {
        assert(i >= 0 && i < num_items_ && d >= 0 && d < num_dims_);
        return item_sizes_[i * num_dims_ + d];
    }

    /// Conflict neighbours of item i.
    [[nodiscard]] std::vector<int> const& conflicts(int i) const {
        assert(i >= 0 && i < num_items_);
        return conflict_adj_[i];
    }

    /// Whether two items conflict.
    [[nodiscard]] bool has_conflict(int i, int j) const {
        assert(i >= 0 && i < num_items_ && j >= 0 && j < num_items_);
        auto const& adj = conflict_adj_[i];
        return std::find(adj.begin(), adj.end(), j) != adj.end();
    }

    /// Continuous lower bound: ceil(sum of item sizes / bin capacity) per dim,
    /// taking the max across dimensions. Uses bin type 0.
    [[nodiscard]] int continuous_lower_bound() const noexcept { return continuous_lb_; }

    /// L2 lower bound (Martello & Toth, 1990) for single-dimension case
    /// using bin type 0. Falls back to continuous LB for multi-dim.
    [[nodiscard]] int l2_lower_bound() const noexcept { return l2_lb_; }

private:
    int num_bin_types_ = 0;
    int num_items_ = 0;
    int num_dims_ = 0;

    // Flat arrays: bin_capacities_[b * num_dims_ + d], item_sizes_[i * num_dims_ + d].
    std::vector<int> bin_capacities_;
    std::vector<int> bin_costs_;
    std::vector<int> bin_counts_;
    std::vector<int> item_sizes_;

    // Adjacency-list conflict graph.
    std::vector<std::vector<int>> conflict_adj_;

    // Lower bounds.
    int continuous_lb_ = 0;
    int l2_lb_ = 0;

    void compute_lower_bounds_() {
        if (num_items_ == 0 || num_bin_types_ == 0) {
            return;
        }

        // Continuous lower bound: max over dimensions of ceil(sum_sizes / capacity).
        // Use bin type 0 as the reference bin.
        int clb = 0;
        for (int d = 0; d < num_dims_; ++d) {
            int total_size = 0;
            for (int i = 0; i < num_items_; ++i) {
                total_size += item_sizes_[i * num_dims_ + d];
            }
            int cap = bin_capacities_[0 * num_dims_ + d];
            if (cap > 0) {
                int lb = (total_size + cap - 1) / cap;
                clb = std::max(clb, lb);
            }
        }
        continuous_lb_ = clb;

        // L2 lower bound (for 1D only, using bin type 0).
        if (num_dims_ == 1) {
            int cap = bin_capacities_[0];
            int half = cap / 2;

            // Count items > half capacity.
            int large_count = 0;
            int remaining_space = 0;
            int small_total = 0;

            for (int i = 0; i < num_items_; ++i) {
                int s = item_sizes_[i];
                if (s > half) {
                    ++large_count;
                    remaining_space += cap - s;
                } else {
                    small_total += s;
                }
            }

            // Small items that don't fit in remaining space of large bins.
            int unfilled = std::max(0, small_total - remaining_space);
            int extra_bins = (unfilled + cap - 1) / cap;
            l2_lb_ = std::max(clb, large_count + extra_bins);
        } else {
            l2_lb_ = clb;  // multi-dim: fall back to continuous LB
        }
    }
};

}  // namespace coso
