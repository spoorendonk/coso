#include "routing/operators/relocate_with_depot.h"

#include <cassert>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

int RelocateWithDepot::find_empty_route(Solution const& sol, ProblemData const& data,
                                        int vehicle_type, int exclude_route) {
    // Walk through all routes, find one that matches the vehicle type and is
    // empty.
    for (int r = 0; r < sol.num_routes(); ++r) {
        if (r == exclude_route) {
            continue;
        }
        if (sol.route(r).vehicle_type() == vehicle_type && sol.route(r).empty()) {
            return r;
        }
    }
    return -1;
}

int RelocateWithDepot::count_trips(Solution const& sol, ProblemData const& data, int vehicle_type) {
    int count = 0;
    for (int r = 0; r < sol.num_routes(); ++r) {
        if (sol.route(r).vehicle_type() == vehicle_type && !sol.route(r).empty()) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
//  find_best_move
// ---------------------------------------------------------------------------

bool RelocateWithDepot::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                       ProblemData const& data) {
    best_delta_ = 0;

    // --- Try SPLIT moves: split an overloaded route into two ---
    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);
        if (route.size() < 2) {
            continue;
        }

        auto const& vt = data.vehicle_type(route.vehicle_type());

        // Only vehicles with reload_depot >= 0 support multi-trip.
        if (vt.reload_depot < 0) {
            continue;
        }

        // Check max_reloads: count current trips for this vehicle type.
        // Each non-empty route of this type is a trip.  A split adds one trip.
        // max_reloads limits the number of depot visits (= trips - 1).
        // So total trips must be <= max_reloads + 1 (or max_reloads == 0 = unlimited).
        int current_trips = count_trips(sol, data, route.vehicle_type());
        if (vt.max_reloads > 0 && current_trips >= vt.max_reloads + 1) {
            continue;
        }

        // Find an empty route slot for the second half.
        int empty = find_empty_route(sol, data, route.vehicle_type(), r);
        if (empty < 0) {
            continue;  // No empty slot available.
        }

        // Current cost of this route (the empty route has zero cost).
        int64_t old_cost = eval.route_cost(route);

        // Try every split point: clients [0..pos) in route, [pos..n) in new route.
        for (int pos = 1; pos < route.size(); ++pos) {
            // Build the two sub-sequences and evaluate their costs.
            // First half: clients [0..pos).
            // Second half: clients [pos..n).

            // We use a temporary Route to evaluate each half.
            Route first_half(data, route.vehicle_type());
            {
                std::vector<int> clients;
                clients.reserve(pos);
                for (int i = 0; i < pos; ++i) {
                    clients.push_back(route.client(i));
                }
                first_half.set_clients(std::move(clients));
            }

            Route second_half(data, route.vehicle_type());
            {
                std::vector<int> clients;
                clients.reserve(route.size() - pos);
                for (int i = pos; i < route.size(); ++i) {
                    clients.push_back(route.client(i));
                }
                second_half.set_clients(std::move(clients));
            }

            int64_t new_cost = eval.route_cost(first_half) + eval.route_cost(second_half);
            int64_t delta = new_cost - old_cost;

            if (delta < best_delta_) {
                best_delta_ = delta;
                move_type_ = MoveType::SPLIT;
                route_a_ = r;
                split_pos_ = pos;
                empty_route_ = empty;
            }
        }
    }

    // --- Try MERGE moves: merge two same-type routes ---
    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.empty()) {
            continue;
        }

        auto const& vt = data.vehicle_type(route_a.vehicle_type());
        if (vt.reload_depot < 0) {
            continue;
        }

        for (int rb = ra + 1; rb < sol.num_routes(); ++rb) {
            auto const& route_b = sol.route(rb);
            if (route_b.empty()) {
                continue;
            }
            if (route_b.vehicle_type() != route_a.vehicle_type()) {
                continue;
            }

            // Current cost of both routes.
            int64_t old_cost = eval.route_cost(route_a) + eval.route_cost(route_b);

            // Merged route: route_a clients followed by route_b clients.
            Route merged(data, route_a.vehicle_type());
            {
                std::vector<int> clients;
                clients.reserve(route_a.size() + route_b.size());
                for (int i = 0; i < route_a.size(); ++i) {
                    clients.push_back(route_a.client(i));
                }
                for (int i = 0; i < route_b.size(); ++i) {
                    clients.push_back(route_b.client(i));
                }
                merged.set_clients(std::move(clients));
            }

            int64_t new_cost = eval.route_cost(merged);
            int64_t delta = new_cost - old_cost;

            if (delta < best_delta_) {
                best_delta_ = delta;
                move_type_ = MoveType::MERGE;
                route_a_ = ra;
                route_b_ = rb;
            }

            // Also try the reverse order: route_b then route_a.
            Route merged_rev(data, route_a.vehicle_type());
            {
                std::vector<int> clients;
                clients.reserve(route_a.size() + route_b.size());
                for (int i = 0; i < route_b.size(); ++i) {
                    clients.push_back(route_b.client(i));
                }
                for (int i = 0; i < route_a.size(); ++i) {
                    clients.push_back(route_a.client(i));
                }
                merged_rev.set_clients(std::move(clients));
            }

            int64_t new_cost_rev = eval.route_cost(merged_rev);
            int64_t delta_rev = new_cost_rev - old_cost;

            if (delta_rev < best_delta_) {
                best_delta_ = delta_rev;
                move_type_ = MoveType::MERGE;
                // Store in reverse: route_b_ first, route_a_ second.
                // In apply, we append route_a_'s clients after route_b_'s.
                route_a_ = rb;
                route_b_ = ra;
            }
        }
    }

    return best_delta_ < 0;
}

// ---------------------------------------------------------------------------
//  apply
// ---------------------------------------------------------------------------

void RelocateWithDepot::apply(Solution& sol) const {
    assert(best_delta_ < 0);

    switch (move_type_) {
        case MoveType::SPLIT: {
            auto const& route = sol.route(route_a_);
            int n = route.size();

            // Build client sequences for both halves.
            std::vector<int> first_clients;
            std::vector<int> second_clients;
            first_clients.reserve(split_pos_);
            second_clients.reserve(n - split_pos_);

            for (int i = 0; i < split_pos_; ++i) {
                first_clients.push_back(route.client(i));
            }
            for (int i = split_pos_; i < n; ++i) {
                second_clients.push_back(route.client(i));
            }

            // Apply via set_route_clients (updates assignment tracking).
            sol.set_route_clients(route_a_, std::move(first_clients));
            sol.set_route_clients(empty_route_, std::move(second_clients));
            break;
        }
        case MoveType::MERGE: {
            auto const& ra = sol.route(route_a_);
            auto const& rb = sol.route(route_b_);

            // Build merged client sequence: route_a then route_b.
            std::vector<int> merged;
            merged.reserve(ra.size() + rb.size());
            for (int i = 0; i < ra.size(); ++i) {
                merged.push_back(ra.client(i));
            }
            for (int i = 0; i < rb.size(); ++i) {
                merged.push_back(rb.client(i));
            }

            // Clear route_b first (so clients become unassigned), then set route_a.
            sol.set_route_clients(route_b_, {});
            sol.set_route_clients(route_a_, std::move(merged));
            break;
        }
    }
}

}  // namespace coso
