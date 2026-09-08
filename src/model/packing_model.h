#pragma once

#include "types.h"

#include <utility>
#include <vector>

namespace coso {

/// Parameters for an item.
struct ItemParams {
    std::vector<int> size;  ///< N dimensions (matches bin capacity dimensions)
};

/// Packing model: declare the bin capacity, items, constraints, then solve.
///
/// Supports bin packing, vector bin packing (multiple dimensions),
/// and bin packing with conflicts.
class PackingModel {
public:
    /// Set the capacity of the (single, unlimited) bin type.
    void set_bin_capacity(std::vector<int> capacity);

    /// Add an item with the given parameters.
    int add_item(ItemParams p);

    // -- Constraints ---------------------------------------------------------

    /// Add a conflict: two items cannot share the same bin.
    void add_conflict(int item_a, int item_b);

    // -- Solve ---------------------------------------------------------------

    /// Solve the packing problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_items() const noexcept { return static_cast<int>(items_.size()); }
    [[nodiscard]] int num_dimensions() const noexcept { return num_dims_; }

    [[nodiscard]] std::vector<int> const& bin_capacity() const noexcept { return bin_capacity_; }
    [[nodiscard]] ItemParams const& item(int i) const { return items_[i]; }
    [[nodiscard]] auto const& conflicts() const noexcept { return conflicts_; }

private:
    std::vector<int> bin_capacity_;
    std::vector<ItemParams> items_;
    std::vector<std::pair<int, int>> conflicts_;
    int num_dims_ = 0;
};

}  // namespace coso
