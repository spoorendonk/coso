#include "routing/operators/relocate_with_depot.h"

#include <cassert>

namespace coso {

// ---------------------------------------------------------------------------
//  Static helpers
// ---------------------------------------------------------------------------

std::vector<std::vector<int>>
RelocateWithDepot::split_into_trips(std::vector<int> const& clients)
{
    std::vector<std::vector<int>> trips;
    trips.emplace_back();

    for (int c : clients) {
        if (c == DEPOT_VISIT) {
            trips.emplace_back();
        } else {
            trips.back().push_back(c);
        }
    }
    return trips;
}

int RelocateWithDepot::count_depot_visits(std::vector<int> const& clients)
{
    int count = 0;
    for (int c : clients) {
        if (c == DEPOT_VISIT)
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
//  find_best_move
// ---------------------------------------------------------------------------

bool RelocateWithDepot::find_best_move(Solution const& sol,
                                        CostEvaluator const& eval,
                                        ProblemData const& data)
{
    best_delta_ = 0;
    route_ = -1;

    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);
        if (route.size() < 2)
            continue;

        auto const& vt = data.vehicle_type(route.vehicle_type());

        // Get the current client sequence (may contain DEPOT_VISIT markers).
        std::vector<int> clients(route.clients().begin(),
                                  route.clients().end());
        int num_depot_visits = count_depot_visits(clients);

        // Current cost of this route.
        int64_t old_cost = eval.route_cost(route);

        // --- Try inserting a depot visit at each position ---
        // Only if the vehicle type supports reloads and we haven't hit the limit.
        if (vt.max_reloads > 0 && num_depot_visits < vt.max_reloads) {
            for (int pos = 1; pos < static_cast<int>(clients.size()); ++pos) {
                // Don't insert depot next to another depot visit.
                if (clients[pos] == DEPOT_VISIT ||
                    clients[pos - 1] == DEPOT_VISIT)
                    continue;

                // Build new client list with depot visit inserted at pos.
                std::vector<int> new_clients;
                new_clients.reserve(clients.size() + 1);
                for (int i = 0; i < pos; ++i)
                    new_clients.push_back(clients[i]);
                new_clients.push_back(DEPOT_VISIT);
                for (int i = pos; i < static_cast<int>(clients.size()); ++i)
                    new_clients.push_back(clients[i]);

                // Evaluate new route cost.
                Route temp(data, route.vehicle_type());
                temp.set_clients(std::move(new_clients));
                int64_t new_cost = eval.route_cost(temp);
                int64_t delta = new_cost - old_cost;

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = kInsertDepot;
                    route_ = r;
                    pos_ = pos;
                }
            }
        }

        // --- Try removing each existing depot visit ---
        for (int pos = 0; pos < static_cast<int>(clients.size()); ++pos) {
            if (clients[pos] != DEPOT_VISIT)
                continue;

            // Build new client list without the depot visit at pos.
            std::vector<int> new_clients;
            new_clients.reserve(clients.size() - 1);
            for (int i = 0; i < static_cast<int>(clients.size()); ++i) {
                if (i != pos)
                    new_clients.push_back(clients[i]);
            }

            Route temp(data, route.vehicle_type());
            temp.set_clients(std::move(new_clients));
            int64_t new_cost = eval.route_cost(temp);
            int64_t delta = new_cost - old_cost;

            if (delta < best_delta_) {
                best_delta_ = delta;
                move_type_ = kRemoveDepot;
                route_ = r;
                pos_ = pos;
            }
        }
    }

    return best_delta_ < 0;
}

// ---------------------------------------------------------------------------
//  apply
// ---------------------------------------------------------------------------

void RelocateWithDepot::apply(Solution& sol) const
{
    assert(route_ >= 0);

    auto const& route = sol.route(route_);
    std::vector<int> clients(route.clients().begin(), route.clients().end());

    if (move_type_ == kInsertDepot) {
        // Insert DEPOT_VISIT at pos_.
        clients.insert(clients.begin() + pos_, DEPOT_VISIT);
    } else {
        assert(move_type_ == kRemoveDepot);
        assert(clients[pos_] == DEPOT_VISIT);
        clients.erase(clients.begin() + pos_);
    }

    sol.set_route_clients(route_, std::move(clients));
}

} // namespace coso
