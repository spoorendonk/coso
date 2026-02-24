#pragma once

#include "packing/packing_data.h"
#include "packing/packing_solution.h"

#include <vector>

namespace coso {

/// Multi-dimensional bin capacity tracker with heuristic bin-selection
/// methods (first-fit, best-fit) and analytics (utilization, lower bounds).
///
/// Wraps a PackingSolution and provides higher-level queries that scan
/// across bins. All mutating operations delegate to the underlying solution,
/// keeping load vectors in sync automatically.
class BinCapacity {
public:
    /// Construct a tracker wrapping the given solution.
    explicit BinCapacity(PackingSolution& sol);

    // -------------------------------------------------------------------
    //  Per-bin queries (delegate to solution with extra analytics)
    // -------------------------------------------------------------------

    /// Whether item fits in bin across all dimensions (capacity only).
    [[nodiscard]] bool fits(int bin, int item) const;

    /// Remaining capacity of bin in dimension d.
    [[nodiscard]] int residual(int bin, int dim) const;

    /// Average utilization of bin across all dimensions (0.0 -- 1.0).
    /// Returns 0.0 for empty bins.
    [[nodiscard]] double utilization(int bin) const;

    /// Total utilization: weighted average across all used bins.
    [[nodiscard]] double total_utilization() const;

    // -------------------------------------------------------------------
    //  Incremental updates (O(D) per call)
    // -------------------------------------------------------------------

    /// Add item to bin. O(D) where D = number of dimensions.
    void add_item(int bin, int item);

    /// Remove item from bin. O(D).
    void remove_item(int bin, int item);

    // -------------------------------------------------------------------
    //  Heuristic bin selection
    // -------------------------------------------------------------------

    /// Find the first bin where item fits (capacity only). Returns -1 if
    /// no bin has sufficient capacity.
    [[nodiscard]] int first_fit(int item) const;

    /// Find the bin where item fits with the least total residual capacity
    /// (sum across dimensions) after placement. Returns -1 if no bin fits.
    [[nodiscard]] int best_fit(int item) const;

    // -------------------------------------------------------------------
    //  Lower bounds
    // -------------------------------------------------------------------

    /// Continuous lower bound on the number of bins needed:
    ///   max over dimensions of ceil(sum_item_sizes / bin_capacity).
    /// Uses bin type 0 as the reference.
    [[nodiscard]] int continuous_lower_bound() const;

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The underlying solution.
    [[nodiscard]] PackingSolution& solution() noexcept { return *sol_; }
    [[nodiscard]] PackingSolution const& solution() const noexcept {
        return *sol_;
    }

private:
    PackingSolution* sol_;
};

} // namespace coso
