#pragma once

#include "routing/piecewise_cost.h"
#include "routing/route.h"

#include <cstdint>
#include <memory>

namespace coso {

/// Evaluates the cost of routes and solutions.
///
/// Computes two components:
///   1. **Objective**: distance cost + fixed vehicle cost + duration cost
///      - prize credits for served optional clients.
///   2. **Penalties**: constraint violations weighted by penalty parameters:
///      - capacity violation * load_penalty
///      - (future: time warp * tw_penalty, distance excess * dist_penalty)
///
/// Penalized cost = objective + penalties.  During search, infeasible solutions
/// are allowed but penalized.  Penalty weights are adjusted by a penalty
/// manager (separate component) targeting a configurable feasibility ratio.
///
/// Supports fast delta evaluation: compute cost change from a single insert
/// or remove without recomputing the full solution cost.
class CostEvaluator {
public:
    /// Construct a CostEvaluator with the given penalty weights.
    ///
    /// @param load_penalty   Penalty per unit of load excess.
    /// @param tw_penalty     Penalty per unit of time warp (future).
    /// @param dist_penalty   Penalty per unit of distance excess (future).
    explicit CostEvaluator(int load_penalty = 100, int tw_penalty = 100, int dist_penalty = 100);

    // -------------------------------------------------------------------
    //  Penalty weight accessors / mutators
    // -------------------------------------------------------------------

    [[nodiscard]] int load_penalty() const noexcept { return load_penalty_; }
    [[nodiscard]] int tw_penalty() const noexcept { return tw_penalty_; }
    [[nodiscard]] int dist_penalty() const noexcept { return dist_penalty_; }

    void set_load_penalty(int p) noexcept { load_penalty_ = p; }
    void set_tw_penalty(int p) noexcept { tw_penalty_ = p; }
    void set_dist_penalty(int p) noexcept { dist_penalty_ = p; }

    // -------------------------------------------------------------------
    //  Piecewise linear cost functions
    // -------------------------------------------------------------------

    /// Set a piecewise linear function for distance costs.
    ///
    /// When set, this replaces the linear `distance * unit_distance_cost`
    /// computation.  The function maps total route distance to cost.
    void set_distance_cost_function(PiecewiseLinearFunction func);

    /// Set a piecewise linear function for duration costs.
    ///
    /// When set, this replaces the linear `duration * unit_duration_cost`
    /// computation.  The function maps total route duration to cost.
    void set_duration_cost_function(PiecewiseLinearFunction func);

    /// Clear the piecewise distance cost function (revert to linear).
    void clear_distance_cost_function() noexcept;

    /// Clear the piecewise duration cost function (revert to linear).
    void clear_duration_cost_function() noexcept;

    /// Whether a piecewise distance cost function is set.
    [[nodiscard]] bool has_distance_cost_function() const noexcept;

    /// Whether a piecewise duration cost function is set.
    [[nodiscard]] bool has_duration_cost_function() const noexcept;

    // -------------------------------------------------------------------
    //  Route-level cost evaluation
    // -------------------------------------------------------------------

    /// Compute the objective cost of a single route (no penalties).
    ///
    /// objective = distance * unit_distance_cost
    ///           + fixed_cost (if route is non-empty)
    ///           - sum of prizes for served clients
    [[nodiscard]] int64_t route_objective(Route const& route) const;

    /// Compute the penalty cost of a single route.
    ///
    /// penalty = load_excess * load_penalty
    [[nodiscard]] int64_t route_penalty(Route const& route) const;

    /// Penalized cost = objective + penalty.
    [[nodiscard]] int64_t route_cost(Route const& route) const;

    // -------------------------------------------------------------------
    //  Delta evaluation helpers
    // -------------------------------------------------------------------

    /// Cost change from inserting a client into a route at the given position.
    /// Combines distance delta and load penalty delta.
    ///
    /// @param route   The current route (before insertion).
    /// @param pos     Position to insert at (0..route.size()).
    /// @param client  Client index to insert.
    /// @return Delta cost (positive = more expensive).
    [[nodiscard]] int64_t eval_insert_cost(Route const& route, int pos, int client) const;

    /// Cost change from removing a client from a route at the given position.
    ///
    /// @param route   The current route (before removal).
    /// @param pos     Position to remove (0..route.size()-1).
    /// @return Delta cost (negative = cheaper).
    [[nodiscard]] int64_t eval_remove_cost(Route const& route, int pos) const;

private:
    int load_penalty_;
    int tw_penalty_;
    int dist_penalty_;

    /// Optional piecewise cost functions (nullptr = use linear).
    std::unique_ptr<PiecewiseLinearFunction> distance_cost_func_;
    std::unique_ptr<PiecewiseLinearFunction> duration_cost_func_;

    /// Compute distance cost using piecewise or linear evaluation.
    [[nodiscard]] int64_t distance_cost_(int distance, CostParams const& cost) const;

    /// Compute duration cost using piecewise or linear evaluation.
    [[nodiscard]] int64_t duration_cost_(int duration, CostParams const& cost) const;
};

}  // namespace coso
