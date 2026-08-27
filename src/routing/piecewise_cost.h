#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace coso {

/// A piecewise linear function defined by breakpoints.
///
/// Given breakpoints (x0, y0), (x1, y1), ..., (xn, yn) with strictly
/// increasing x-values, the function linearly interpolates between adjacent
/// breakpoints and linearly extrapolates beyond the first/last breakpoint.
///
/// Common use cases:
///   - **Tiered pricing**: first 100km at rate A, next 200km at rate B.
///   - **Overtime penalties**: normal hours free, overtime at increasing rates.
///
/// All arithmetic uses integers to match the routing engine's cost model.
class PiecewiseLinearFunction {
public:
    /// A single breakpoint (x, y) on the piecewise linear function.
    struct Breakpoint {
        int x;
        int y;
    };

    /// Construct from a list of breakpoints.
    ///
    /// Breakpoints must be sorted by strictly increasing x.
    /// At least two breakpoints are required to define a function.
    ///
    /// @param breakpoints  Sorted list of (x, y) pairs.
    explicit PiecewiseLinearFunction(std::vector<Breakpoint> breakpoints);

    /// Evaluate the function at a given x-value.
    ///
    /// Uses binary search to find the segment, then linear interpolation.
    /// Extrapolates linearly beyond the first/last breakpoint using the
    /// slope of the nearest segment.
    ///
    /// @param x  Input value.
    /// @return   Interpolated (or extrapolated) y-value.
    [[nodiscard]] int64_t evaluate(int x) const;

    /// Compute the cost change when x changes from x_old to x_new.
    ///
    /// Equivalent to evaluate(x_new) - evaluate(x_old), but optimized
    /// to O(1) when both values lie on the same segment.
    ///
    /// @param x_old  Previous x-value.
    /// @param x_new  New x-value.
    /// @return       Change in function value (positive = more costly).
    [[nodiscard]] int64_t delta(int x_old, int x_new) const;

    /// Number of breakpoints.
    [[nodiscard]] int num_breakpoints() const noexcept {
        return static_cast<int>(breakpoints_.size());
    }

    /// Read-only access to breakpoints.
    [[nodiscard]] std::vector<Breakpoint> const& breakpoints() const noexcept {
        return breakpoints_;
    }

    // -------------------------------------------------------------------
    //  Factory methods for common patterns
    // -------------------------------------------------------------------

    /// Create a tiered pricing function.
    ///
    /// Example: tiered({100, 200}, {1, 2, 3}) means:
    ///   - First 100 units at rate 1 per unit
    ///   - Next 100 units (100..200) at rate 2 per unit
    ///   - Beyond 200 units at rate 3 per unit
    ///
    /// @param thresholds  Sorted tier boundaries (cumulative x-values).
    /// @param rates       Rate (slope) for each tier. Size = thresholds.size() + 1.
    /// @return PiecewiseLinearFunction implementing the tiered pricing.
    static PiecewiseLinearFunction tiered(std::vector<int> const& thresholds,
                                          std::vector<int> const& rates);

    /// Create an overtime penalty function.
    ///
    /// Normal hours up to `normal_limit` cost zero.
    /// Overtime beyond `normal_limit` costs `overtime_rate` per unit.
    /// If `max_overtime` > 0, overtime beyond normal_limit + max_overtime
    /// costs `excess_rate` per unit.
    ///
    /// @param normal_limit   Free hours threshold.
    /// @param overtime_rate   Cost per unit of overtime.
    /// @param max_overtime    Size of first overtime tier (0 = single tier).
    /// @param excess_rate     Cost per unit beyond max_overtime (if > 0).
    /// @return PiecewiseLinearFunction implementing overtime penalties.
    static PiecewiseLinearFunction overtime(int normal_limit, int overtime_rate,
                                            int max_overtime = 0, int excess_rate = 0);

private:
    std::vector<Breakpoint> breakpoints_;

    /// Find the segment index for a given x-value.
    /// Returns the index i such that breakpoints_[i].x <= x < breakpoints_[i+1].x,
    /// or 0 if x < breakpoints_[0].x, or n-2 if x >= breakpoints_[n-1].x.
    [[nodiscard]] int find_segment_(int x) const;
};

}  // namespace coso
