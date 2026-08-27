#pragma once

#include "types.h"

#include <utility>
#include <vector>

namespace coso {

/// Parameters for a bin type.
struct BinTypeParams {
    std::vector<int> capacity;  ///< N dimensions (weight, volume, etc.)
    int cost = 1;               ///< cost per bin used
    int count = 0;              ///< 0 = unlimited
};

/// Parameters for an item.
struct ItemParams {
    std::vector<int> size;  ///< N dimensions (matches bin capacity dimensions)
};

/// Packing model: declare bin types, items, constraints, then solve.
///
/// Supports bin packing, vector bin packing (multiple dimensions),
/// and bin packing with conflicts.
class PackingModel {
public:
    /// Add a bin type with the given parameters.
    int add_bin_type(BinTypeParams p);

    /// Add an item with the given parameters.
    int add_item(ItemParams p);

    // -- Constraints ---------------------------------------------------------

    /// Add a conflict: two items cannot share the same bin.
    void add_conflict(int item_a, int item_b);

    // -- Objective -----------------------------------------------------------

    /// Set objective to minimize total number of bins (weighted by cost).
    void minimize_bins();

    // -- Solve ---------------------------------------------------------------

    /// Solve the packing problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_bin_types() const noexcept { return static_cast<int>(bin_types_.size()); }
    [[nodiscard]] int num_items() const noexcept { return static_cast<int>(items_.size()); }
    [[nodiscard]] int num_dimensions() const noexcept { return num_dims_; }

    [[nodiscard]] BinTypeParams const& bin_type(int b) const { return bin_types_[b]; }
    [[nodiscard]] ItemParams const& item(int i) const { return items_[i]; }
    [[nodiscard]] auto const& conflicts() const noexcept { return conflicts_; }

private:
    std::vector<BinTypeParams> bin_types_;
    std::vector<ItemParams> items_;
    std::vector<std::pair<int, int>> conflicts_;
    int num_dims_ = 0;
    bool minimize_bins_ = false;
};

}  // namespace coso
