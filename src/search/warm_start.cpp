#include "search/warm_start.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <format>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  PinSet
// ---------------------------------------------------------------------------

PinSet::PinSet(int num_clients)
    : is_pinned_(num_clients, false)
{
}

void PinSet::pin(int client)
{
    if (client < 0)
        throw std::invalid_argument(
            std::format("PinSet::pin: invalid client index {}", client));

    // Grow if needed.
    if (client >= static_cast<int>(is_pinned_.size()))
        is_pinned_.resize(client + 1, false);

    if (is_pinned_[client])
        return;  // Already pinned.

    is_pinned_[client] = true;

    // Insert into sorted list.
    auto it = std::lower_bound(pinned_.begin(), pinned_.end(), client);
    pinned_.insert(it, client);
}

void PinSet::unpin(int client)
{
    if (client < 0 || client >= static_cast<int>(is_pinned_.size()))
        return;

    if (!is_pinned_[client])
        return;

    is_pinned_[client] = false;

    auto it = std::lower_bound(pinned_.begin(), pinned_.end(), client);
    if (it != pinned_.end() && *it == client)
        pinned_.erase(it);
}

bool PinSet::is_pinned(int client) const
{
    if (client < 0 || client >= static_cast<int>(is_pinned_.size()))
        return false;
    return is_pinned_[client];
}

void PinSet::clear()
{
    std::fill(is_pinned_.begin(), is_pinned_.end(), false);
    pinned_.clear();
}

// ---------------------------------------------------------------------------
//  warm_start
// ---------------------------------------------------------------------------

Solution warm_start(
    std::vector<std::vector<int>> const& routes,
    ProblemData const& data,
    [[maybe_unused]] CostEvaluator const& eval)
{
    int const num_clients = data.num_clients();
    int const total_vehicles = data.total_vehicles();

    // Validate number of routes.
    if (static_cast<int>(routes.size()) > total_vehicles) {
        throw std::invalid_argument(std::format(
            "warm_start: {} routes provided but only {} vehicles available",
            routes.size(), total_vehicles));
    }

    // Track which clients have been assigned (for duplicate detection).
    std::vector<bool> seen(num_clients, false);

    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        for (int client : routes[r]) {
            // Validate client index.
            if (client < 0 || client >= num_clients) {
                throw std::invalid_argument(std::format(
                    "warm_start: client index {} out of range [0, {}) "
                    "in route {}",
                    client, num_clients, r));
            }

            // Check for duplicates.
            if (seen[client]) {
                throw std::invalid_argument(std::format(
                    "warm_start: client {} appears more than once "
                    "(duplicate in route {})",
                    client, r));
            }
            seen[client] = true;
        }
    }

    // Check that all required clients are assigned.
    for (int c = 0; c < num_clients; ++c) {
        if (!seen[c] && data.client(c).required) {
            throw std::invalid_argument(std::format(
                "warm_start: required client {} is not assigned to any route",
                c));
        }
    }

    // Build the solution.
    Solution sol(data);

    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty()) {
            sol.set_route_clients(r, routes[r]);
        }
    }

    return sol;
}

// ---------------------------------------------------------------------------
//  cheapest_insert
// ---------------------------------------------------------------------------

bool cheapest_insert(Solution& sol,
                     int client,
                     ProblemData const& data,
                     CostEvaluator const& eval)
{
    (void)data;

    int best_route = -1;
    int best_pos = -1;
    int64_t best_delta = INT64_MAX;

    for (int r = 0; r < sol.num_routes(); ++r) {
        Route const& route = sol.route(r);
        int n = route.size();

        // Evaluate inserting at every position 0..n.
        for (int pos = 0; pos <= n; ++pos) {
            int64_t delta = eval.eval_insert_cost(route, pos, client);
            if (delta < best_delta) {
                best_delta = delta;
                best_route = r;
                best_pos = pos;
            }
        }
    }

    if (best_route < 0)
        return false;

    sol.insert_client(best_route, best_pos, client);
    return true;
}

// ---------------------------------------------------------------------------
//  local_search_with_pins — pin-aware first-improvement descent
// ---------------------------------------------------------------------------

namespace {

/// Check if a segment of a route contains any pinned clients.
bool segment_has_pinned(Route const& route, int start, int count,
                        PinSet const& pins)
{
    for (int i = start; i < start + count && i < route.size(); ++i) {
        if (pins.is_pinned(route.client(i)))
            return true;
    }
    return false;
}

/// Try Exchange(1,0) with pin checks.  Returns true if an improving move
/// was found and applied.  Uses exact cost computation to prevent cycling.
bool try_relocate_with_pins(Solution& sol, CostEvaluator const& eval,
                            ProblemData const& data, PinSet const& pins)
{
    int64_t best_delta = 0;
    int best_from_route = -1, best_from_pos = -1;
    int best_to_route = -1, best_to_pos = -1;
    int best_client = -1;

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        Route const& routeA = sol.route(ra);

        for (int pa = 0; pa < routeA.size(); ++pa) {
            int client = routeA.client(pa);
            if (pins.is_pinned(client))
                continue;

            for (int rb = 0; rb < sol.num_routes(); ++rb) {
                Route const& routeB = sol.route(rb);
                int n = routeB.size();

                for (int pb = 0; pb <= n; ++pb) {
                    // Skip identity move.
                    if (ra == rb && (pb == pa || pb == pa + 1))
                        continue;

                    int64_t delta;
                    if (ra == rb) {
                        // Intra-route: build modified route and compare.
                        int64_t cost_before = eval.route_cost(routeA);
                        auto clients = std::vector<int>(
                            routeA.clients().begin(), routeA.clients().end());
                        // Remove from pa, insert at adjusted position.
                        clients.erase(clients.begin() + pa);
                        int adj = (pb > pa) ? pb - 1 : pb;
                        clients.insert(clients.begin() + adj, client);
                        Route tmp(data, routeA.vehicle_type());
                        tmp.set_clients(std::move(clients));
                        delta = eval.route_cost(tmp) - cost_before;
                    } else {
                        // Inter-route: delta from separate remove and insert.
                        delta = eval.eval_remove_cost(routeA, pa)
                              + eval.eval_insert_cost(routeB, pb, client);
                    }

                    if (delta < best_delta) {
                        best_delta = delta;
                        best_from_route = ra;
                        best_from_pos = pa;
                        best_to_route = rb;
                        best_to_pos = pb;
                        best_client = client;
                    }
                }
            }
        }
    }

    if (best_delta >= 0)
        return false;

    // Apply the move: remove then insert, adjusting position if needed.
    sol.remove_client(best_from_route, best_from_pos);

    int adj_pos = best_to_pos;
    if (best_from_route == best_to_route && best_from_pos < best_to_pos)
        --adj_pos;

    sol.insert_client(best_to_route, adj_pos, best_client);
    return true;
}

/// Try Exchange(1,1) with pin checks.  Uses full cost recomputation to
/// avoid approximate delta errors that could cause cycling.
bool try_swap_with_pins(Solution& sol, CostEvaluator const& eval,
                        ProblemData const& data, PinSet const& pins)
{
    int64_t best_delta = 0;
    int best_ra = -1, best_pa = -1;
    int best_rb = -1, best_pb = -1;

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        Route const& routeA = sol.route(ra);

        for (int pa = 0; pa < routeA.size(); ++pa) {
            int clientA = routeA.client(pa);
            if (pins.is_pinned(clientA))
                continue;

            for (int rb = ra; rb < sol.num_routes(); ++rb) {
                Route const& routeB = sol.route(rb);
                int start_pb = (ra == rb) ? pa + 1 : 0;

                for (int pb = start_pb; pb < routeB.size(); ++pb) {
                    int clientB = routeB.client(pb);
                    if (pins.is_pinned(clientB))
                        continue;

                    // Compute exact delta by building modified client lists
                    // and evaluating the cost difference.
                    int64_t cost_before = eval.route_cost(routeA);
                    if (ra != rb)
                        cost_before += eval.route_cost(routeB);

                    // Build modified client lists.
                    auto clients_a = std::vector<int>(
                        routeA.clients().begin(), routeA.clients().end());
                    auto clients_b = std::vector<int>(
                        routeB.clients().begin(), routeB.clients().end());

                    if (ra == rb) {
                        std::swap(clients_a[pa], clients_a[pb]);
                        // Temporarily set and evaluate.
                        Route tmp_a(data, routeA.vehicle_type());
                        tmp_a.set_clients(std::move(clients_a));
                        int64_t cost_after = eval.route_cost(tmp_a);
                        int64_t delta = cost_after - cost_before;
                        if (delta < best_delta) {
                            best_delta = delta;
                            best_ra = ra;
                            best_pa = pa;
                            best_rb = rb;
                            best_pb = pb;
                        }
                    } else {
                        clients_a[pa] = clientB;
                        clients_b[pb] = clientA;
                        Route tmp_a(data, routeA.vehicle_type());
                        Route tmp_b(data, routeB.vehicle_type());
                        tmp_a.set_clients(std::move(clients_a));
                        tmp_b.set_clients(std::move(clients_b));
                        int64_t cost_after = eval.route_cost(tmp_a)
                                           + eval.route_cost(tmp_b);
                        int64_t delta = cost_after - cost_before;
                        if (delta < best_delta) {
                            best_delta = delta;
                            best_ra = ra;
                            best_pa = pa;
                            best_rb = rb;
                            best_pb = pb;
                        }
                    }
                }
            }
        }
    }

    if (best_delta >= 0)
        return false;

    // Apply swap by replacing clients at their positions.
    int clientA = sol.route(best_ra).client(best_pa);
    int clientB = sol.route(best_rb).client(best_pb);

    auto clients_a = std::vector<int>(sol.route(best_ra).clients().begin(),
                                      sol.route(best_ra).clients().end());
    auto clients_b = std::vector<int>(sol.route(best_rb).clients().begin(),
                                      sol.route(best_rb).clients().end());

    if (best_ra == best_rb) {
        std::swap(clients_a[best_pa], clients_a[best_pb]);
        sol.set_route_clients(best_ra, std::move(clients_a));
    } else {
        clients_a[best_pa] = clientB;
        clients_b[best_pb] = clientA;
        sol.set_route_clients(best_ra, std::move(clients_a));
        sol.set_route_clients(best_rb, std::move(clients_b));
    }

    return true;
}

} // anonymous namespace

void local_search_with_pins(Solution& sol,
                            PinSet const& pins,
                            ProblemData const& data,
                            CostEvaluator const& eval)
{
    bool improved = true;
    while (improved) {
        improved = false;

        // Try relocate (Exchange 1,0).
        if (try_relocate_with_pins(sol, eval, data, pins)) {
            improved = true;
            continue;
        }

        // Try swap (Exchange 1,1).
        if (try_swap_with_pins(sol, eval, data, pins)) {
            improved = true;
            continue;
        }
    }
}

// ---------------------------------------------------------------------------
//  replan
// ---------------------------------------------------------------------------

PinSet replan(Solution& sol,
              ReplanConfig const& config,
              ProblemData const& data,
              CostEvaluator const& eval)
{
    // Validate: pinned and removed must not overlap.
    for (int c : config.removed_clients) {
        if (config.pinned_clients.contains(c)) {
            throw std::invalid_argument(std::format(
                "replan: client {} is both pinned and marked for removal", c));
        }
    }

    // Step 1: Remove specified clients.
    for (int client : config.removed_clients) {
        // Find and remove from its route.
        bool found = false;
        for (int r = 0; r < sol.num_routes() && !found; ++r) {
            Route const& route = sol.route(r);
            for (int p = 0; p < route.size(); ++p) {
                if (route.client(p) == client) {
                    sol.remove_client(r, p);
                    found = true;
                    break;
                }
            }
        }
        if (!found && sol.is_assigned(client)) {
            throw std::invalid_argument(std::format(
                "replan: client {} marked for removal but not found in routes",
                client));
        }
    }

    // Step 2: Insert new clients using cheapest insertion.
    for (int client : config.new_clients) {
        if (sol.is_assigned(client)) {
            throw std::invalid_argument(std::format(
                "replan: new client {} is already assigned in the solution",
                client));
        }
        cheapest_insert(sol, client, data, eval);
    }

    // Step 3: Build PinSet.
    PinSet pins(data.num_clients());
    for (int c : config.pinned_clients) {
        if (c < 0 || c >= data.num_clients()) {
            throw std::invalid_argument(std::format(
                "replan: pinned client {} out of range [0, {})",
                c, data.num_clients()));
        }
        pins.pin(c);
    }

    // Step 4: Run local search, skipping pinned moves.
    local_search_with_pins(sol, pins, data, eval);

    return pins;
}

} // namespace coso
