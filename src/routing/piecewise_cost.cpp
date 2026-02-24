#include "routing/piecewise_cost.h"

#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

PiecewiseLinearFunction::PiecewiseLinearFunction(std::vector<Breakpoint> breakpoints)
    : breakpoints_(std::move(breakpoints))
{
    if (breakpoints_.size() < 2)
        throw std::invalid_argument(
            "PiecewiseLinearFunction requires at least 2 breakpoints");

    // Verify strictly increasing x-values.
    for (size_t i = 1; i < breakpoints_.size(); ++i) {
        if (breakpoints_[i].x <= breakpoints_[i - 1].x)
            throw std::invalid_argument(
                "PiecewiseLinearFunction breakpoints must have strictly "
                "increasing x-values");
    }
}

// ---------------------------------------------------------------------------
//  Segment lookup
// ---------------------------------------------------------------------------

int PiecewiseLinearFunction::find_segment_(int x) const
{
    int n = static_cast<int>(breakpoints_.size());

    // Clamp to valid segment range.
    if (x <= breakpoints_[0].x)
        return 0;
    if (x >= breakpoints_[n - 1].x)
        return n - 2;

    // Binary search: find the largest i such that breakpoints_[i].x <= x.
    // We want the segment [i, i+1] where breakpoints_[i].x <= x < breakpoints_[i+1].x.
    int lo = 0;
    int hi = n - 2;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if (breakpoints_[mid].x <= x)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

// ---------------------------------------------------------------------------
//  Evaluation
// ---------------------------------------------------------------------------

int64_t PiecewiseLinearFunction::evaluate(int x) const
{
    int seg = find_segment_(x);
    auto const& p0 = breakpoints_[seg];
    auto const& p1 = breakpoints_[seg + 1];

    // Linear interpolation/extrapolation:
    //   y = p0.y + (x - p0.x) * (p1.y - p0.y) / (p1.x - p0.x)
    //
    // Use 64-bit arithmetic to avoid overflow in the numerator.
    int64_t dx = p1.x - p0.x;
    int64_t dy = p1.y - p0.y;
    int64_t offset = static_cast<int64_t>(x) - p0.x;

    return p0.y + (offset * dy) / dx;
}

int64_t PiecewiseLinearFunction::delta(int x_old, int x_new) const
{
    if (x_old == x_new)
        return 0;

    int seg_old = find_segment_(x_old);
    int seg_new = find_segment_(x_new);

    // Fast path: both on the same segment -> O(1) with a single slope.
    if (seg_old == seg_new) {
        auto const& p0 = breakpoints_[seg_old];
        auto const& p1 = breakpoints_[seg_old + 1];

        int64_t dx = p1.x - p0.x;
        int64_t dy = p1.y - p0.y;
        int64_t x_delta = static_cast<int64_t>(x_new) - x_old;

        return (x_delta * dy) / dx;
    }

    // General case: evaluate both.
    return evaluate(x_new) - evaluate(x_old);
}

// ---------------------------------------------------------------------------
//  Factory methods
// ---------------------------------------------------------------------------

PiecewiseLinearFunction
PiecewiseLinearFunction::tiered(std::vector<int> const& thresholds,
                                std::vector<int> const& rates)
{
    if (rates.size() != thresholds.size() + 1)
        throw std::invalid_argument(
            "tiered: rates.size() must equal thresholds.size() + 1");

    // Build breakpoints starting from (0, 0).
    std::vector<Breakpoint> bps;
    bps.reserve(thresholds.size() + 2);

    bps.push_back({0, 0});

    int64_t cumulative_y = 0;
    int prev_x = 0;

    for (size_t i = 0; i < thresholds.size(); ++i) {
        int x = thresholds[i];
        cumulative_y += static_cast<int64_t>(x - prev_x) * rates[i];
        bps.push_back({x, static_cast<int>(cumulative_y)});
        prev_x = x;
    }

    // Add a final breakpoint far enough to establish the last tier's slope.
    // We pick prev_x + 1 with y = cumulative_y + rates.back().
    int final_x = prev_x + 1;
    int final_y = static_cast<int>(cumulative_y + rates.back());
    bps.push_back({final_x, final_y});

    return PiecewiseLinearFunction(std::move(bps));
}

PiecewiseLinearFunction
PiecewiseLinearFunction::overtime(int normal_limit,
                                  int overtime_rate,
                                  int max_overtime,
                                  int excess_rate)
{
    std::vector<Breakpoint> bps;

    // (0, 0): zero cost at zero input.
    // (normal_limit, 0): zero cost up to the limit (slope = 0).
    bps.push_back({0, 0});
    bps.push_back({normal_limit, 0});

    if (max_overtime > 0 && excess_rate > 0) {
        // Two overtime tiers.
        int ot_end = normal_limit + max_overtime;
        int ot_cost = max_overtime * overtime_rate;
        bps.push_back({ot_end, ot_cost});

        // Final point to establish the excess slope.
        bps.push_back({ot_end + 1, ot_cost + excess_rate});
    } else {
        // Single overtime tier.
        bps.push_back({normal_limit + 1, overtime_rate});
    }

    return PiecewiseLinearFunction(std::move(bps));
}

} // namespace coso
