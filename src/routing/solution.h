#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/route.h"

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace coso {

/// A complete solution to a routing problem.
///
/// Contains one Route per vehicle (total_vehicles routes, some may be empty).
/// Tracks which clients are unassigned (not in any route).
///
/// Supports:
///   - Cost evaluation via CostEvaluator (total cost across all routes).
///   - Copy/clone for population-based algorithms (HGS).
///   - Modification: assign/remove clients from routes.
///
/// Vehicle numbering: vehicles are numbered 0..total_vehicles-1, where
/// vehicles for type t start at the offset sum of counts of types < t.
class Solution {
public:
    /// Construct a solution with all clients unassigned.
    ///
    /// Creates one empty Route per vehicle across all vehicle types.
    explicit Solution(ProblemData const& data);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The problem data this solution belongs to.
    [[nodiscard]] ProblemData const& data() const noexcept { return *data_; }

    /// Number of routes (= total vehicles).
    [[nodiscard]] int num_routes() const noexcept {
        return static_cast<int>(routes_.size());
    }

    /// Access route by vehicle index.
    [[nodiscard]] Route const& route(int vehicle) const {
        assert(vehicle >= 0 && vehicle < num_routes());
        return routes_[vehicle];
    }

    /// Mutable access to route by vehicle index.
    [[nodiscard]] Route& route(int vehicle) {
        assert(vehicle >= 0 && vehicle < num_routes());
        return routes_[vehicle];
    }

    /// Read-only access to all routes.
    [[nodiscard]] std::span<Route const> routes() const noexcept {
        return routes_;
    }

    /// Number of unassigned clients.
    [[nodiscard]] int num_unassigned() const noexcept {
        return static_cast<int>(unassigned_.size());
    }

    /// The set of unassigned client indices (unsorted).
    [[nodiscard]] std::span<int const> unassigned() const noexcept {
        return unassigned_;
    }

    /// Whether a client is currently assigned to some route.
    [[nodiscard]] bool is_assigned(int client) const {
        assert(client >= 0 && client < data_->num_clients());
        return assigned_[client];
    }

    /// Number of non-empty routes (routes with at least one client).
    [[nodiscard]] int num_used_vehicles() const noexcept;

    // -------------------------------------------------------------------
    //  Cost evaluation
    // -------------------------------------------------------------------

    /// Total penalized cost across all routes.
    [[nodiscard]] int64_t cost(CostEvaluator const& eval) const;

    /// Total objective (no penalties) across all routes.
    [[nodiscard]] int64_t objective(CostEvaluator const& eval) const;

    /// Total penalty across all routes.
    [[nodiscard]] int64_t penalty(CostEvaluator const& eval) const;

    /// Total distance across all routes.
    [[nodiscard]] int total_distance() const noexcept;

    /// Whether the solution is feasible (no constraint violations).
    [[nodiscard]] bool feasible() const noexcept;

    // -------------------------------------------------------------------
    //  Modification
    // -------------------------------------------------------------------

    /// Set the client sequence for a route and update assignment tracking.
    ///
    /// Any clients previously in the route that are not in the new sequence
    /// become unassigned.  Clients in the new sequence are marked assigned.
    void set_route_clients(int vehicle, std::vector<int> clients);

    /// Insert a client into a route at the given position.
    /// The client must currently be unassigned.
    void insert_client(int vehicle, int pos, int client);

    /// Remove the client at the given position from a route.
    /// The client becomes unassigned.
    void remove_client(int vehicle, int pos);

private:
    ProblemData const* data_;
    std::vector<Route> routes_;
    std::vector<int> unassigned_;     ///< Unassigned client indices.
    std::vector<bool> assigned_;      ///< assigned_[client] = true if in some route.

    /// Rebuild the unassigned_ list from the assigned_ flags.
    void rebuild_unassigned_();
};

} // namespace coso
