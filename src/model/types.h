#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace coso {

/// 2D coordinate (e.g., depot or client location).
struct Coord {
    double x, y;
};

/// Numeric type of every routing distance, duration, time-window bound, service time and
/// distance/duration budget, from the schema through the compiled matrices to the engine's route
/// state, and of the search costs built from them. Chosen when compiling, with the CMake option
/// COSO_MEASURE (#249): `int` is the default, exact on integer data and the fastest; `int64` is for
/// scaled data that would overflow it; `double` is the build that reproduces double-precision
/// published values (Solomon's and Gehring & Homberger's SINTEF tables, Chao's TOP budgets).
#if defined(COSO_MEASURE_DOUBLE)
using Measure = double;
using Cost = double;
#elif defined(COSO_MEASURE_INT64)
using Measure = std::int64_t;
using Cost = std::int64_t;
#else
using Measure = int;
using Cost = std::int64_t;
#endif

/// Upper sentinel for an open time window ("no deadline").
inline constexpr Measure kMeasureMax = std::numeric_limits<Measure>::max();

/// Sentinel for "no cost seen yet" in a minimisation.
inline constexpr Cost kCostMax = std::numeric_limits<Cost>::max();

/// How far a double-build excess or cost may be from its exact value through accumulated rounding.
/// A prefix/suffix merge and a recomputed sum differ by a few ulps of the largest route total, and
/// a Gehring & Homberger total is about 1e5, whose ulp is about 1e-11. Penalty weights multiply a
/// measure, so the cost tolerance is the wider one. Both are far below the two decimals published
/// values carry. Integer builds compare exactly.
inline constexpr double kMeasureTolerance = 1e-7;
inline constexpr double kCostTolerance = 1e-6;

/// Whether a non-negative excess (time warp, distance excess) counts as no violation.
[[nodiscard]] constexpr bool is_zero_excess(Measure excess) noexcept {
    if constexpr (std::is_floating_point_v<Measure>) {
        return excess <= kMeasureTolerance;
    } else {
        return excess == 0;
    }
}

/// Whether two measures are the same value up to accumulated rounding.
[[nodiscard]] constexpr bool measures_match(Measure a, Measure b) noexcept {
    if constexpr (std::is_floating_point_v<Measure>) {
        return a - b <= kMeasureTolerance && b - a <= kMeasureTolerance;
    } else {
        return a == b;
    }
}

/// Whether two costs are the same value up to accumulated rounding.  A penalised cost can reach
/// 1e8, so past the absolute tolerance the allowance grows with the magnitude.
[[nodiscard]] constexpr bool costs_match(Cost a, Cost b) noexcept {
    if constexpr (std::is_floating_point_v<Cost>) {
        Cost const magnitude = std::max(a < 0 ? -a : a, b < 0 ? -b : b);
        Cost const allowed = std::max(kCostTolerance, 1e-9 * magnitude);
        return a - b <= allowed && b - a <= allowed;
    } else {
        return a == b;
    }
}

/// Whether a move's cost delta is a real improvement.  In the double build a delta inside the
/// tolerance is rounding, and accepting it could make a descent cycle forever.
[[nodiscard]] constexpr bool is_improving(Cost delta) noexcept {
    if constexpr (std::is_floating_point_v<Cost>) {
        return delta < -kCostTolerance;
    } else {
        return delta < 0;
    }
}

/// Time window constraint: service must start within [start, end].
struct TimeWindow {
    Measure start, end;
};

/// Stop criterion passed to solve().
struct TimeLimit {
    double seconds;
    double work_units = 0.0;  ///< 0.0 = no deterministic work limit
    explicit TimeLimit(double s, double wu = 0.0) : seconds(s), work_units(wu) {}
};

/// Common result fields shared across all engines.
///
/// Engine-specific data is stored in typed vectors and accessed through
/// convenience methods.  Only the relevant engine populates its section:
///   - routing:    routes(), unserved(), distance(), prize(), num_routes()
///   - scheduling: makespan(), schedule()
///   - rostering:  roster(), day(), understaffed()
///   - packing:    bins(), num_bins(), unassigned()
///   - network:    arc_flows()
///   - lotsizing:  production(), inventory()
struct Result {
    bool feasible_ = false;
    double cost_ = 0.0;
    double elapsed_seconds_ = 0.0;
    int iterations_ = 0;
    uint64_t work_ticks_ = 0;
    double work_units_ = 0.0;

    [[nodiscard]] bool feasible() const noexcept { return feasible_; }
    [[nodiscard]] double cost() const noexcept { return cost_; }
    [[nodiscard]] double elapsed_seconds() const noexcept { return elapsed_seconds_; }
    [[nodiscard]] int iterations() const noexcept { return iterations_; }
    [[nodiscard]] uint64_t work_ticks() const noexcept { return work_ticks_; }
    [[nodiscard]] double work_units() const noexcept { return work_units_; }

    // -- Routing ---------------------------------------------------------

    /// Each inner vector is a route: an ordered sequence of client ids.
    std::vector<std::vector<int>> routes_;
    /// Clients that could not be served (optional / infeasible).
    std::vector<int> unserved_;
    /// Total distance travelled by the returned routes.
    double distance_ = 0.0;
    /// Summed prize of the served clients.  cost() is the declared objective
    /// (RoutingObjective says which of distance_, prize_ and num_routes() it
    /// is built from).
    double prize_ = 0.0;
    /// Start depot of each route: one entry per entry of routes_, in the same
    /// order, holding a depot index in 0..num_depots-1.
    std::vector<int> route_start_depots_;
    /// The depots the solution opened, ascending: the distinct start depots of
    /// the returned non-empty routes.  A depot no route starts from is not
    /// opened, whatever its declared fixed cost.
    std::vector<int> opened_depots_;

    [[nodiscard]] auto const& routes() const noexcept { return routes_; }
    [[nodiscard]] auto const& unserved() const noexcept { return unserved_; }
    [[nodiscard]] double distance() const noexcept { return distance_; }
    [[nodiscard]] double prize() const noexcept { return prize_; }
    /// Number of non-empty routes, i.e. vehicles used.
    [[nodiscard]] int num_routes() const noexcept { return static_cast<int>(routes_.size()); }
    [[nodiscard]] auto const& route_start_depots() const noexcept { return route_start_depots_; }
    [[nodiscard]] auto const& opened_depots() const noexcept { return opened_depots_; }

    // -- Scheduling ------------------------------------------------------

    /// Per-operation: (machine, start_time).
    struct OpSchedule {
        int machine = -1;
        int start_time = 0;
    };
    std::vector<OpSchedule> schedule_;
    int makespan_ = 0;

    [[nodiscard]] auto const& schedule() const noexcept { return schedule_; }
    [[nodiscard]] int makespan() const noexcept { return makespan_; }

    // -- Rostering (nurse rostering, employee scheduling) ----------------

    struct RosterEntry {
        int employee = -1;
        int shift = -1;
        std::string employee_name;
        std::string shift_name;
    };
    /// Indexed by day: roster_[day] is that day's roster entries.
    std::vector<std::vector<RosterEntry>> roster_;
    /// Packing: the items placed in no bin.
    std::vector<int> unassigned_;

    /// A cover demand staffed below its min_employees: the demand on
    /// (shift, day) for `skill`, empty for one with no required skill.  Each
    /// worker counts toward one skill demand on a cell (#255), so a cell
    /// with several demands may list several entries.
    struct CoverShortfall {
        int shift = -1;
        int day = -1;
        std::string skill;
    };
    /// Cover demands left short.  A soft cell's shortfall is priced into
    /// cost(); a hard cell's also makes the result infeasible.  Excess over
    /// max_employees is not listed: a soft cell's shows in cost() only, a
    /// hard cell's in feasible() and cost().
    std::vector<CoverShortfall> understaffed_;

    [[nodiscard]] auto const& roster() const noexcept { return roster_; }
    [[nodiscard]] auto const& unassigned() const noexcept { return unassigned_; }
    [[nodiscard]] auto const& understaffed() const noexcept { return understaffed_; }
    /// Convenience: the roster for a given day.
    [[nodiscard]] auto const& day(int d) const { return roster_.at(d); }

    // -- Packing ---------------------------------------------------------

    /// Each inner vector is a bin: the item ids placed in that bin.
    std::vector<std::vector<int>> bins_;

    [[nodiscard]] auto const& bins() const noexcept { return bins_; }
    [[nodiscard]] int num_bins() const noexcept { return static_cast<int>(bins_.size()); }

    // -- Network / Flow --------------------------------------------------

    /// arc_flows_[commodity][arc] = flow on that arc, indexed as the arcs were
    /// declared.  This is the exact solution: it can be checked against every
    /// supply and capacity the model declared.  No path view is returned — a
    /// node sequence cannot tell parallel arcs apart, and decomposing flow
    /// into paths belongs with #176's accessors (#206).
    ///
    /// The flows are a solution only when feasible().  An infeasible result
    /// holds either no flows at all (the model has no arcs, or the stop
    /// criterion fired before the solver ran) or the partial flow the solver
    /// had routed when it stopped: within arc capacities but not conserving
    /// flow, with supply left unrouted, demand unmet, or both.
    std::vector<std::vector<double>> arc_flows_;

    [[nodiscard]] auto const& arc_flows() const noexcept { return arc_flows_; }

    // -- Lot sizing / production -----------------------------------------

    /// production_quantities_[product][period] = produced quantity.
    std::vector<std::vector<double>> production_quantities_;
    /// inventory_levels_[product][period] = end-of-period inventory.
    std::vector<std::vector<double>> inventory_levels_;

    [[nodiscard]] auto const& production() const noexcept { return production_quantities_; }
    [[nodiscard]] auto const& inventory() const noexcept { return inventory_levels_; }
};

}  // namespace coso
