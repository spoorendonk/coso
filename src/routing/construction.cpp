#include "routing/construction.h"

#include "routing/resources/load_resource.h"

#include <algorithm>
#include <vector>

namespace coso::construction {

// ---------------------------------------------------------------------------
//  Helper: check if a merged load state fits within a vehicle type's capacity.
// ---------------------------------------------------------------------------

static bool load_fits(LoadResource::State const& state, ProblemData::VehicleTypeData const& vt) {
    return LoadResource::excess(state, vt) == 0;
}

// ---------------------------------------------------------------------------
//  Nearest-neighbour
// ---------------------------------------------------------------------------

Solution nearest_neighbour(ProblemData const& data, CostEvaluator const& eval) {
    Solution sol(data);

    int num_clients = data.num_clients();
    int num_depots = data.num_depots();

    // Track which clients are still unvisited.
    std::vector<bool> visited(num_clients, false);
    int remaining = num_clients;

    // Iterate over available vehicles.
    for (int v = 0; v < sol.num_routes() && remaining > 0; ++v) {
        auto& route = sol.route(v);
        int vtype = route.vehicle_type();
        int profile = data.vehicle_type(vtype).profile;
        int depot = 0;  // depot node index

        // Build route for this vehicle.
        std::vector<int> route_clients;
        int current_node = depot;

        while (remaining > 0) {
            // Find the nearest unvisited client that fits.
            int best_client = -1;
            int best_dist = INT_MAX;

            for (int c = 0; c < num_clients; ++c) {
                if (visited[c]) {
                    continue;
                }

                int client_node = num_depots + c;
                int d = data.dist(profile, current_node, client_node);

                if (d < best_dist) {
                    // Check capacity: build tentative route state to verify.
                    // We use the route's eval_insert_load which considers the
                    // full prefix/suffix load state.
                    if (route.eval_insert_load(route.size(), c) == 0) {
                        best_dist = d;
                        best_client = c;
                    }
                }
            }

            if (best_client < 0) {
                break;  // No more clients fit in this route.
            }

            // Add the best client.
            route.insert(route.size(), best_client);
            route_clients.push_back(best_client);
            visited[best_client] = true;
            --remaining;
            current_node = num_depots + best_client;
        }

        // Update solution assignment tracking.
        // We already inserted into the route directly; now sync the solution.
        // Reset route and use set_route_clients for proper tracking.
        route.set_clients({});  // Clear direct modifications.
        if (!route_clients.empty()) {
            sol.set_route_clients(v, std::move(route_clients));
        }
    }

    return sol;
}

// ---------------------------------------------------------------------------
//  Clarke-Wright savings
// ---------------------------------------------------------------------------

Solution clarke_wright(ProblemData const& data, CostEvaluator const& eval) {
    int num_clients = data.num_clients();
    int num_depots = data.num_depots();
    int depot = 0;  // depot node index

    if (num_clients == 0) {
        return Solution(data);
    }

    // Use profile 0 for savings computation (default).
    // For heterogeneous fleets we use the first vehicle type's profile.
    int profile = 0;
    if (data.num_vehicle_types() > 0) {
        profile = data.vehicle_type(0).profile;
    }

    // Step 1: Each client starts in its own "route" (a list of clients).
    // route_of[c] = which route index client c is in.
    // routes[r] = vector of clients in route r.
    // route_vtype[r] = vehicle type assigned to route r.
    // We start with num_clients routes, each a singleton.

    struct CWRoute {
        std::vector<int> clients;
        LoadResource::State load_state;
        int vtype = -1;  // not yet assigned to a vehicle type
    };

    std::vector<CWRoute> routes(num_clients);
    std::vector<int> route_of(num_clients);  // client -> route index

    for (int c = 0; c < num_clients; ++c) {
        routes[c].clients = {c};
        routes[c].load_state = LoadResource::init(data, c);
        route_of[c] = c;
    }

    // Step 2: Compute savings for all client pairs.
    struct Saving {
        int i, j;   // client indices
        int value;  // saving amount
    };

    std::vector<Saving> savings;
    savings.reserve(static_cast<size_t>(num_clients) * (num_clients - 1) / 2);

    for (int i = 0; i < num_clients; ++i) {
        int node_i = num_depots + i;
        int di = data.dist(profile, depot, node_i);

        for (int j = i + 1; j < num_clients; ++j) {
            int node_j = num_depots + j;
            int dj = data.dist(profile, depot, node_j);
            int dij = data.dist(profile, node_i, node_j);

            int s = di + dj - dij;
            if (s > 0) {
                savings.push_back({i, j, s});
            }
        }
    }

    // Sort by decreasing savings.
    std::sort(savings.begin(), savings.end(),
              [](Saving const& a, Saving const& b) { return a.value > b.value; });

    // Step 3: Process savings and merge routes.
    for (auto const& [ci, cj, sval] : savings) {
        int ri = route_of[ci];
        int rj = route_of[cj];

        // Skip if already in the same route.
        if (ri == rj) {
            continue;
        }

        // Check that ci is at the end of route ri and cj is at the start
        // of route rj, or vice versa.  This ensures we only merge at
        // route endpoints.
        auto& route_i = routes[ri];
        auto& route_j = routes[rj];

        bool i_at_end = (!route_i.clients.empty() && route_i.clients.back() == ci);
        bool j_at_start = (!route_j.clients.empty() && route_j.clients.front() == cj);
        bool j_at_end = (!route_j.clients.empty() && route_j.clients.back() == cj);
        bool i_at_start = (!route_i.clients.empty() && route_i.clients.front() == ci);

        // Determine merge direction.
        // We want to append one route to the other.
        int from_route = -1, to_route = -1;
        bool reverse_from = false;

        if (i_at_end && j_at_start) {
            // Append route_j to the end of route_i.
            from_route = rj;
            to_route = ri;
        } else if (j_at_end && i_at_start) {
            // Append route_i to the end of route_j.
            from_route = ri;
            to_route = rj;
        } else if (i_at_end && j_at_end) {
            // Reverse route_j, then append to route_i.
            from_route = rj;
            to_route = ri;
            reverse_from = true;
        } else if (i_at_start && j_at_start) {
            // Reverse route_i, then append route_j.
            from_route = rj;
            to_route = ri;
            // Reverse the "to" route instead.
            std::reverse(routes[ri].clients.begin(), routes[ri].clients.end());
            // Load state is direction-independent for LoadResource.
        } else {
            continue;  // Neither client is at a route endpoint.
        }

        if (reverse_from) {
            std::reverse(routes[from_route].clients.begin(), routes[from_route].clients.end());
        }

        // Check capacity of merged route.
        auto merged_load =
            LoadResource::merge(routes[to_route].load_state, routes[from_route].load_state);

        // Find a vehicle type that can handle the merged load.
        bool found_vtype = false;
        for (int t = 0; t < data.num_vehicle_types(); ++t) {
            if (load_fits(merged_load, data.vehicle_type(t))) {
                found_vtype = true;
                break;
            }
        }
        if (!found_vtype) {
            continue;
        }

        // Merge: append from_route clients to to_route.
        auto& to = routes[to_route];
        auto& from = routes[from_route];

        for (int c : from.clients) {
            to.clients.push_back(c);
            route_of[c] = to_route;
        }
        to.load_state = merged_load;

        from.clients.clear();
        from.load_state = LoadResource::init_depot(data);
    }

    // Step 5: Assign merged routes to vehicles and build the Solution.
    Solution sol(data);

    // Collect non-empty routes.
    std::vector<int> nonempty;
    for (int r = 0; r < num_clients; ++r) {
        if (!routes[r].clients.empty()) {
            nonempty.push_back(r);
        }
    }

    // Sort non-empty routes by decreasing load (assign biggest routes to
    // biggest vehicles first for best fit).
    std::sort(nonempty.begin(), nonempty.end(), [&](int a, int b) {
        // Compare by total demand in first dimension.
        int load_a = 0, load_b = 0;
        if (!routes[a].load_state.dims.empty()) {
            load_a = routes[a].load_state.dims[0].delivery + routes[a].load_state.dims[0].pickup;
        }
        if (!routes[b].load_state.dims.empty()) {
            load_b = routes[b].load_state.dims[0].delivery + routes[b].load_state.dims[0].pickup;
        }
        return load_a > load_b;
    });

    // Sort vehicle types by decreasing capacity for best-fit assignment.
    struct VehicleSlot {
        int vehicle_idx;  // index in Solution's route array
        int vtype;
    };
    std::vector<VehicleSlot> slots;
    {
        int offset = 0;
        for (int t = 0; t < data.num_vehicle_types(); ++t) {
            int count = data.vehicle_type(t).count;
            for (int v = 0; v < count; ++v) {
                slots.push_back({offset + v, t});
            }
            offset += count;
        }
    }

    // Sort slots by decreasing capacity (first dimension).
    std::sort(slots.begin(), slots.end(), [&](VehicleSlot const& a, VehicleSlot const& b) {
        auto const& vta = data.vehicle_type(a.vtype);
        auto const& vtb = data.vehicle_type(b.vtype);
        int cap_a = vta.capacity.empty() ? 0 : vta.capacity[0];
        int cap_b = vtb.capacity.empty() ? 0 : vtb.capacity[0];
        return cap_a > cap_b;
    });

    std::vector<bool> slot_used(slots.size(), false);

    for (int ri : nonempty) {
        auto const& cw_route = routes[ri];

        // Find the first unused slot whose vehicle type can handle this load.
        for (size_t s = 0; s < slots.size(); ++s) {
            if (slot_used[s]) {
                continue;
            }

            auto const& vt = data.vehicle_type(slots[s].vtype);
            if (load_fits(cw_route.load_state, vt)) {
                sol.set_route_clients(slots[s].vehicle_idx, cw_route.clients);
                slot_used[s] = true;
                break;
            }
        }
    }

    return sol;
}

}  // namespace coso::construction
