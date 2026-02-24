#include "packing/bin_capacity.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <numeric>

namespace coso {

BinCapacity::BinCapacity(PackingSolution& sol)
    : sol_(&sol)
{
}

// ---------------------------------------------------------------------------
//  Per-bin queries
// ---------------------------------------------------------------------------

bool BinCapacity::fits(int bin, int item) const
{
    return sol_->item_fits_capacity(item, bin);
}

int BinCapacity::residual(int bin, int dim) const
{
    return sol_->bin_remaining(bin, dim);
}

double BinCapacity::utilization(int bin) const
{
    auto const& data = sol_->data();
    int D = data.num_dims();
    int bt = sol_->bin_type(bin);

    double total = 0.0;
    for (int d = 0; d < D; ++d) {
        int cap = data.bin_capacity(bt, d);
        if (cap > 0) {
            total += static_cast<double>(sol_->bin_load(bin, d))
                   / static_cast<double>(cap);
        }
    }
    return (D > 0) ? total / D : 0.0;
}

double BinCapacity::total_utilization() const
{
    int num_bins = sol_->num_bins();
    double sum = 0.0;
    int used = 0;

    for (int b = 0; b < num_bins; ++b) {
        if (!sol_->bin_items(b).empty()) {
            sum += utilization(b);
            ++used;
        }
    }
    return (used > 0) ? sum / used : 0.0;
}

// ---------------------------------------------------------------------------
//  Incremental updates
// ---------------------------------------------------------------------------

void BinCapacity::add_item(int bin, int item)
{
    sol_->assign(item, bin);
}

void BinCapacity::remove_item(int bin, int item)
{
    assert(sol_->item_bin(item) == bin);
    sol_->unassign(item);
}

// ---------------------------------------------------------------------------
//  Heuristic bin selection
// ---------------------------------------------------------------------------

int BinCapacity::first_fit(int item) const
{
    int num_bins = sol_->num_bins();
    for (int b = 0; b < num_bins; ++b) {
        if (sol_->item_fits_capacity(item, b)) {
            return b;
        }
    }
    return -1;
}

int BinCapacity::best_fit(int item) const
{
    auto const& data = sol_->data();
    int D = data.num_dims();
    int num_bins = sol_->num_bins();

    int best_bin = -1;
    int best_residual = std::numeric_limits<int>::max();

    for (int b = 0; b < num_bins; ++b) {
        if (!sol_->item_fits_capacity(item, b))
            continue;

        // Sum of residual capacity across dimensions after placing item.
        int total_residual = 0;
        for (int d = 0; d < D; ++d) {
            total_residual += sol_->bin_remaining(b, d) - data.item_size(item, d);
        }

        if (total_residual < best_residual) {
            best_residual = total_residual;
            best_bin = b;
        }
    }
    return best_bin;
}

// ---------------------------------------------------------------------------
//  Lower bounds
// ---------------------------------------------------------------------------

int BinCapacity::continuous_lower_bound() const
{
    // Delegate to the precomputed value in PackingData.
    return sol_->data().continuous_lower_bound();
}

} // namespace coso
