#include "search/crossover.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <numeric>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Helper: cheapest insertion of a single client into a solution.
// ---------------------------------------------------------------------------

/// Find the (vehicle, position) pair that minimizes insertion cost and insert
/// the client there.  If no feasible position exists, insert at the position
/// with minimum penalized cost (allowing infeasibility).
static void cheapest_insert(Solution& sol,
                            int client,
                            ProblemData const& data,
                            CostEvaluator const& eval)
{
    int best_vehicle = -1;
    int best_pos = -1;
    int64_t best_cost = INT64_MAX;

    for (int v = 0; v < sol.num_routes(); ++v) {
        auto const& route = sol.route(v);
        int n = route.size();

        for (int pos = 0; pos <= n; ++pos) {
            int64_t delta = eval.eval_insert_cost(route, pos, client);
            if (delta < best_cost) {
                best_cost = delta;
                best_vehicle = v;
                best_pos = pos;
            }
        }
    }

    assert(best_vehicle >= 0);
    sol.insert_client(best_vehicle, best_pos, client);
}

// ---------------------------------------------------------------------------
//  SREX crossover
// ---------------------------------------------------------------------------

Solution srex_crossover(Solution const& parent1,
                        Solution const& parent2,
                        ProblemData const& data,
                        CostEvaluator const& eval,
                        std::mt19937& rng,
                        int max_routes)
{
    int num_routes = parent1.num_routes();
    assert(num_routes == parent2.num_routes());
    assert(max_routes >= 1);

    // Step 1: Collect non-empty route indices from parent1.
    std::vector<int> nonempty_p1;
    nonempty_p1.reserve(num_routes);
    for (int v = 0; v < num_routes; ++v) {
        if (!parent1.route(v).empty())
            nonempty_p1.push_back(v);
    }

    if (nonempty_p1.empty()) {
        // parent1 has no routes — return a copy of parent2.
        return parent2;
    }

    // Choose how many routes to select: uniform in [1, min(max_routes, |nonempty|)].
    int upper = std::min(max_routes, static_cast<int>(nonempty_p1.size()));
    std::uniform_int_distribution<int> count_dist(1, upper);
    int num_selected = count_dist(rng);

    // Shuffle and pick the first num_selected non-empty routes.
    std::shuffle(nonempty_p1.begin(), nonempty_p1.end(), rng);
    nonempty_p1.resize(num_selected);

    // Step 2: Collect the set of clients in the selected routes.
    int num_clients = data.num_clients();
    std::vector<bool> in_selected(num_clients, false);

    // Also store the actual client sequences from the selected routes.
    struct SelectedRoute {
        int vehicle_type;
        std::vector<int> clients;
    };
    std::vector<SelectedRoute> selected_routes;
    selected_routes.reserve(num_selected);

    for (int v : nonempty_p1) {
        auto const& route = parent1.route(v);
        SelectedRoute sr;
        sr.vehicle_type = route.vehicle_type();
        sr.clients.reserve(route.size());
        for (int i = 0; i < route.size(); ++i) {
            int c = route.client(i);
            in_selected[c] = true;
            sr.clients.push_back(c);
        }
        selected_routes.push_back(std::move(sr));
    }

    // Step 3: Build offspring starting from parent2.
    // Remove all "selected" clients from parent2's routes.
    Solution offspring(data);

    // Copy parent2 routes, stripping out selected clients.
    for (int v = 0; v < num_routes; ++v) {
        auto const& route = parent2.route(v);
        std::vector<int> kept;
        kept.reserve(route.size());
        for (int i = 0; i < route.size(); ++i) {
            int c = route.client(i);
            if (!in_selected[c])
                kept.push_back(c);
        }
        if (!kept.empty())
            offspring.set_route_clients(v, std::move(kept));
    }

    // Step 4: Insert the selected routes from parent1 into offspring.
    // Find empty vehicle slots (prefer matching vehicle type).
    std::vector<bool> slot_used(num_routes, false);
    for (int v = 0; v < num_routes; ++v) {
        if (!offspring.route(v).empty())
            slot_used[v] = true;
    }

    for (auto& sr : selected_routes) {
        // First pass: find an empty slot with matching vehicle type.
        int best_slot = -1;
        for (int v = 0; v < num_routes; ++v) {
            if (!slot_used[v] &&
                offspring.route(v).vehicle_type() == sr.vehicle_type) {
                best_slot = v;
                break;
            }
        }
        // Second pass: any empty slot.
        if (best_slot < 0) {
            for (int v = 0; v < num_routes; ++v) {
                if (!slot_used[v]) {
                    best_slot = v;
                    break;
                }
            }
        }

        if (best_slot >= 0) {
            offspring.set_route_clients(best_slot, sr.clients);
            slot_used[best_slot] = true;
        } else {
            // No empty slots — insert clients one by one via cheapest insertion.
            for (int c : sr.clients) {
                if (!offspring.is_assigned(c))
                    cheapest_insert(offspring, c, data, eval);
            }
        }
    }

    // Step 5: Any clients still unassigned (were in parent2 routes that got
    // disrupted and not covered by the selected routes) get reinserted.
    // We iterate over a copy because cheapest_insert modifies unassigned().
    std::vector<int> missing(offspring.unassigned().begin(),
                             offspring.unassigned().end());

    // Sort by decreasing demand (first dimension) for better packing.
    std::sort(missing.begin(), missing.end(), [&](int a, int b) {
        auto const& ca = data.client(a);
        auto const& cb = data.client(b);
        int da = ca.demand.empty() ? 0 : ca.demand[0];
        int db = cb.demand.empty() ? 0 : cb.demand[0];
        return da > db;
    });

    for (int c : missing) {
        if (!offspring.is_assigned(c))
            cheapest_insert(offspring, c, data, eval);
    }

    return offspring;
}

} // namespace coso
