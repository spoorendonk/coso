#pragma once

#include "types.h"

#include <vector>

namespace coso {

/// Parameters for a bin type.
struct BinTypeParams {
    std::vector<int> capacity;   ///< N dimensions (weight, volume, etc.)
    int cost  = 1;               ///< cost per bin used
    int count = 0;               ///< 0 = unlimited
};

/// Parameters for an item.
struct ItemParams {
    std::vector<int> size;       ///< N dimensions (matches bin capacity dimensions)
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
};

} // namespace coso
