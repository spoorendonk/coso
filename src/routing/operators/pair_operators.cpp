#include "routing/operators/pair_operators.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <vector>

namespace coso {

namespace {

/// Location of a client in the solution: (route index, position in route).
struct ClientLocation {
    int route;
    int pos;
};

/// Build a lookup table: client -> (route, position).
inline std::vector<ClientLocation> build_client_locations(Solution const& sol)
{
    int nc = sol.data().num_clients();
    std::vector<ClientLocation> loc(nc, {-1, -1});
    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);
        for (int p = 0; p < route.size(); ++p)
            loc[route.client(p)] = {r, p};
    }
    return loc;
}

/// Build a lookup: request index -> route index.
/// Returns -1 if the pickup is not assigned (both pickup and delivery must
/// be on the same route for a valid solution with precedence).
inline std::vector<int> build_request_routes(
    Solution const& sol,
    ProblemData const& data,
    std::vector<ClientLocation> const& locations)
{
    auto const& requests = data.requests();
    std::vector<int> req_routes(requests.size(), -1);
    for (int r = 0; r < static_cast<int>(requests.size()); ++r) {
        auto const& loc = locations[requests[r].pickup];
        if (loc.route >= 0)
            req_routes[r] = loc.route;
    }
    return req_routes;
}

/// Remove clients at the given positions from a route's client list.
/// Positions must be sorted ascending.
inline std::vector<int> remove_clients_at(Route const& route,
                                           std::vector<int> const& positions)
{
    std::vector<int> result;
    result.reserve(route.size() - static_cast<int>(positions.size()));
    int pi = 0;
    for (int i = 0; i < route.size(); ++i) {
        if (pi < static_cast<int>(positions.size()) && i == positions[pi]) {
            ++pi;
        } else {
            result.push_back(route.client(i));
        }
    }
    return result;
}

/// Evaluate the cost of a route with a given client sequence.
inline int64_t eval_route_cost(ProblemData const& data,
                                CostEvaluator const& eval,
                                int vehicle_type,
                                std::vector<int> const& clients)
{
    Route temp(data, vehicle_type);
    temp.set_clients(clients);
    return eval.route_cost(temp);
}

/// Find the best insertion positions for a pickup-delivery pair in a route,
/// trying all (p, d) pairs where p <= d (pickup at position p, delivery at
/// position d+1 in the sequence after pickup is inserted).
///
/// @param base_clients  The client list of the route (without the pair).
/// @param data          Problem data.
/// @param eval          Cost evaluator.
/// @param vehicle_type  Vehicle type of the target route.
/// @param pickup        Pickup client index.
/// @param delivery      Delivery client index.
/// @param[out] best_p   Best pickup insertion position.
/// @param[out] best_d   Best delivery insertion position (in the sequence
///                       after pickup insertion, so always >= best_p + 1).
/// @return The total route cost with the pair inserted at the best positions.
inline int64_t find_best_pair_insertion(
    std::vector<int> const& base_clients,
    ProblemData const& data,
    CostEvaluator const& eval,
    int vehicle_type,
    int pickup, int delivery,
    int& best_p, int& best_d)
{
    int n = static_cast<int>(base_clients.size());
    int64_t best_cost = std::numeric_limits<int64_t>::max();
    best_p = 0;
    best_d = 1;

    // Try inserting pickup at position pp (0..n), then delivery at
    // position dd (pp+1..n+1) in the expanded sequence.
    for (int pp = 0; pp <= n; ++pp) {
        for (int dd = pp + 1; dd <= n + 1; ++dd) {
            // Build the candidate sequence.
            std::vector<int> candidate;
            candidate.reserve(n + 2);
            int bi = 0;
            for (int i = 0; i <= n + 1; ++i) {
                if (i == pp)
                    candidate.push_back(pickup);
                else if (i == dd)
                    candidate.push_back(delivery);
                else if (bi < n)
                    candidate.push_back(base_clients[bi++]);
            }

            int64_t cost = eval_route_cost(data, eval, vehicle_type, candidate);
            if (cost < best_cost) {
                best_cost = cost;
                best_p = pp;
                best_d = dd;
            }
        }
    }

    return best_cost;
}

} // anonymous namespace

// ===========================================================================
//  RelocatePair
// ===========================================================================

bool RelocatePair::find_best_move(Solution const& sol,
                                   CostEvaluator const& eval,
                                   ProblemData const& data)
{
    best_delta_ = 0;
    from_route_ = -1;

    auto const& requests = data.requests();
    if (requests.empty())
        return false;

    auto locations = build_client_locations(sol);
    auto req_routes = build_request_routes(sol, data, locations);

    for (int r = 0; r < static_cast<int>(requests.size()); ++r) {
        int ra = req_routes[r];
        if (ra < 0)
            continue;  // request not assigned

        int pickup = requests[r].pickup;
        int delivery = requests[r].delivery;

        auto const& route_a = sol.route(ra);
        int64_t old_cost_a = eval.route_cost(route_a);

        // Positions of pickup and delivery in the source route.
        int pos_p = locations[pickup].pos;
        int pos_d = locations[delivery].pos;

        // Remove the pair from route A.
        std::vector<int> sorted_pos = {pos_p, pos_d};
        std::sort(sorted_pos.begin(), sorted_pos.end());
        auto reduced_a = remove_clients_at(route_a, sorted_pos);
        int64_t cost_a_without = eval_route_cost(
            data, eval, route_a.vehicle_type(), reduced_a);
        int64_t remove_delta = cost_a_without - old_cost_a;

        // Try inserting into each candidate target route.
        auto try_route = [&](int rb) {
            auto const& route_b = sol.route(rb);
            int64_t old_cost_b = eval.route_cost(route_b);

            std::vector<int> base;
            if (rb == ra) {
                // Intra-route: base is the reduced route (pair removed).
                base = reduced_a;
            } else {
                // Inter-route: base is the full target route.
                base.reserve(route_b.size());
                for (int i = 0; i < route_b.size(); ++i)
                    base.push_back(route_b.client(i));
            }

            int vt = (rb == ra) ? route_a.vehicle_type()
                                : route_b.vehicle_type();

            int bp, bd;
            int64_t new_cost_b = find_best_pair_insertion(
                base, data, eval, vt, pickup, delivery, bp, bd);

            int64_t delta;
            if (rb == ra) {
                // Intra-route: delta = new_cost - old_cost.
                delta = new_cost_b - old_cost_a;
            } else {
                // Inter-route: remove from A + insert into B.
                delta = remove_delta + (new_cost_b - old_cost_b);
            }

            if (delta < best_delta_) {
                best_delta_ = delta;
                request_    = r;
                from_route_ = ra;
                to_route_   = rb;
                pickup_     = pickup;
                delivery_   = delivery;
                insert_p_   = bp;
                insert_d_   = bd;
            }
        };

        // Always try intra-route.
        if (route_a.size() > 2) {  // only if route has more than just the pair
            try_route(ra);
        }

        // Try inter-route: use granular neighbours or all routes.
        if (data.granular_k() > 0) {
            std::vector<bool> route_seen(sol.num_routes(), false);
            route_seen[ra] = true;

            for (int c : {pickup, delivery}) {
                auto nbrs = data.neighbours(c);
                for (int nb_node : nbrs) {
                    if (nb_node < data.num_depots())
                        continue;
                    int nb_client = nb_node - data.num_depots();
                    auto const& loc = locations[nb_client];
                    if (loc.route < 0)
                        continue;
                    int rb = loc.route;
                    if (route_seen[rb])
                        continue;
                    route_seen[rb] = true;
                    try_route(rb);
                }
            }
        } else {
            for (int rb = 0; rb < sol.num_routes(); ++rb) {
                if (rb == ra)
                    continue;
                try_route(rb);
            }
        }
    }

    return best_delta_ < 0;
}

void RelocatePair::apply(Solution& sol) const
{
    assert(from_route_ >= 0);

    if (from_route_ == to_route_) {
        // Intra-route: rebuild from scratch.
        auto const& route = sol.route(from_route_);
        int pos_p = -1, pos_d = -1;
        for (int i = 0; i < route.size(); ++i) {
            if (route.client(i) == pickup_) pos_p = i;
            if (route.client(i) == delivery_) pos_d = i;
        }
        assert(pos_p >= 0 && pos_d >= 0);

        std::vector<int> sorted_pos = {pos_p, pos_d};
        std::sort(sorted_pos.begin(), sorted_pos.end());
        auto base = remove_clients_at(route, sorted_pos);

        // Insert pickup at insert_p_, delivery at insert_d_.
        int n = static_cast<int>(base.size());
        std::vector<int> result;
        result.reserve(n + 2);
        int bi = 0;
        for (int i = 0; i <= n + 1; ++i) {
            if (i == insert_p_)
                result.push_back(pickup_);
            else if (i == insert_d_)
                result.push_back(delivery_);
            else if (bi < n)
                result.push_back(base[bi++]);
        }

        sol.set_route_clients(from_route_, std::move(result));
    } else {
        // Inter-route: remove from source, insert into target.
        auto const& src = sol.route(from_route_);

        // Find current positions.
        int pos_p = -1, pos_d = -1;
        for (int i = 0; i < src.size(); ++i) {
            if (src.client(i) == pickup_) pos_p = i;
            if (src.client(i) == delivery_) pos_d = i;
        }
        assert(pos_p >= 0 && pos_d >= 0);

        // Build new source route (without the pair).
        std::vector<int> sorted_pos = {pos_p, pos_d};
        std::sort(sorted_pos.begin(), sorted_pos.end());
        auto new_src = remove_clients_at(src, sorted_pos);

        // Build new target route (with the pair inserted).
        auto const& tgt = sol.route(to_route_);
        int n = tgt.size();
        std::vector<int> base;
        base.reserve(n);
        for (int i = 0; i < n; ++i)
            base.push_back(tgt.client(i));

        std::vector<int> new_tgt;
        new_tgt.reserve(n + 2);
        int bi = 0;
        for (int i = 0; i <= n + 1; ++i) {
            if (i == insert_p_)
                new_tgt.push_back(pickup_);
            else if (i == insert_d_)
                new_tgt.push_back(delivery_);
            else if (bi < n)
                new_tgt.push_back(base[bi++]);
        }

        // Clear both routes first, then assign.
        sol.set_route_clients(from_route_, {});
        sol.set_route_clients(to_route_, {});
        sol.set_route_clients(from_route_, std::move(new_src));
        sol.set_route_clients(to_route_, std::move(new_tgt));
    }
}

// ===========================================================================
//  SwapPair
// ===========================================================================

bool SwapPair::find_best_move(Solution const& sol,
                               CostEvaluator const& eval,
                               ProblemData const& data)
{
    best_delta_ = 0;
    route_a_ = -1;

    auto const& requests = data.requests();
    if (requests.size() < 2)
        return false;

    auto locations = build_client_locations(sol);
    auto req_routes = build_request_routes(sol, data, locations);

    int num_req = static_cast<int>(requests.size());

    for (int ra_req = 0; ra_req < num_req; ++ra_req) {
        int ra = req_routes[ra_req];
        if (ra < 0)
            continue;

        int pa = requests[ra_req].pickup;
        int da = requests[ra_req].delivery;

        auto const& route_a = sol.route(ra);
        int64_t old_cost_a = eval.route_cost(route_a);

        // Build route A without pair A.
        int pos_pa = locations[pa].pos;
        int pos_da = locations[da].pos;
        std::vector<int> sorted_pos_a = {pos_pa, pos_da};
        std::sort(sorted_pos_a.begin(), sorted_pos_a.end());
        auto reduced_a = remove_clients_at(route_a, sorted_pos_a);

        // Try swapping with each other request.
        auto try_swap_with = [&](int rb_req) {
            if (rb_req <= ra_req)
                return;  // avoid double-counting

            int rb = req_routes[rb_req];
            if (rb < 0 || rb == ra)
                return;  // only inter-route swaps

            int pb = requests[rb_req].pickup;
            int db = requests[rb_req].delivery;

            auto const& route_b = sol.route(rb);
            int64_t old_cost_b = eval.route_cost(route_b);
            int64_t old_total = old_cost_a + old_cost_b;

            // Build route B without pair B.
            int pos_pb = locations[pb].pos;
            int pos_db = locations[db].pos;
            std::vector<int> sorted_pos_b = {pos_pb, pos_db};
            std::sort(sorted_pos_b.begin(), sorted_pos_b.end());
            auto reduced_b = remove_clients_at(route_b, sorted_pos_b);

            // Insert pair B into reduced route A at best positions.
            int bp_b, bd_b;
            int64_t new_cost_a = find_best_pair_insertion(
                reduced_a, data, eval, route_a.vehicle_type(),
                pb, db, bp_b, bd_b);

            // Insert pair A into reduced route B at best positions.
            int bp_a, bd_a;
            int64_t new_cost_b = find_best_pair_insertion(
                reduced_b, data, eval, route_b.vehicle_type(),
                pa, da, bp_a, bd_a);

            int64_t delta = (new_cost_a + new_cost_b) - old_total;

            if (delta < best_delta_) {
                best_delta_  = delta;
                request_a_   = ra_req;
                request_b_   = rb_req;
                route_a_     = ra;
                route_b_     = rb;
                pickup_a_    = pa;
                delivery_a_  = da;
                pickup_b_    = pb;
                delivery_b_  = db;
                insert_pa_   = bp_a;  // pair A goes into route B
                insert_da_   = bd_a;
                insert_pb_   = bp_b;  // pair B goes into route A
                insert_db_   = bd_b;
            }
        };

        // Use granular neighbours to find candidate requests/routes.
        if (data.granular_k() > 0) {
            std::vector<bool> req_seen(num_req, false);
            req_seen[ra_req] = true;

            for (int c : {pa, da}) {
                auto nbrs = data.neighbours(c);
                for (int nb_node : nbrs) {
                    if (nb_node < data.num_depots())
                        continue;
                    int nb_client = nb_node - data.num_depots();
                    auto const& loc = locations[nb_client];
                    if (loc.route < 0 || loc.route == ra)
                        continue;

                    // Find requests on this route.
                    for (int rq = 0; rq < num_req; ++rq) {
                        if (req_seen[rq])
                            continue;
                        if (req_routes[rq] == loc.route) {
                            req_seen[rq] = true;
                            try_swap_with(rq);
                        }
                    }
                }
            }
        } else {
            for (int rb_req = ra_req + 1; rb_req < num_req; ++rb_req)
                try_swap_with(rb_req);
        }
    }

    return best_delta_ < 0;
}

void SwapPair::apply(Solution& sol) const
{
    assert(route_a_ >= 0 && route_b_ >= 0);

    auto const& route_a = sol.route(route_a_);
    auto const& route_b = sol.route(route_b_);

    // Find current positions.
    int pos_pa = -1, pos_da = -1, pos_pb = -1, pos_db = -1;
    for (int i = 0; i < route_a.size(); ++i) {
        if (route_a.client(i) == pickup_a_) pos_pa = i;
        if (route_a.client(i) == delivery_a_) pos_da = i;
    }
    for (int i = 0; i < route_b.size(); ++i) {
        if (route_b.client(i) == pickup_b_) pos_pb = i;
        if (route_b.client(i) == delivery_b_) pos_db = i;
    }
    assert(pos_pa >= 0 && pos_da >= 0 && pos_pb >= 0 && pos_db >= 0);

    // Build reduced routes.
    std::vector<int> spa = {pos_pa, pos_da};
    std::sort(spa.begin(), spa.end());
    auto reduced_a = remove_clients_at(route_a, spa);

    std::vector<int> spb = {pos_pb, pos_db};
    std::sort(spb.begin(), spb.end());
    auto reduced_b = remove_clients_at(route_b, spb);

    // Insert pair B into reduced A.
    int na = static_cast<int>(reduced_a.size());
    std::vector<int> new_a;
    new_a.reserve(na + 2);
    {
        int bi = 0;
        for (int i = 0; i <= na + 1; ++i) {
            if (i == insert_pb_)
                new_a.push_back(pickup_b_);
            else if (i == insert_db_)
                new_a.push_back(delivery_b_);
            else if (bi < na)
                new_a.push_back(reduced_a[bi++]);
        }
    }

    // Insert pair A into reduced B.
    int nb = static_cast<int>(reduced_b.size());
    std::vector<int> new_b;
    new_b.reserve(nb + 2);
    {
        int bi = 0;
        for (int i = 0; i <= nb + 1; ++i) {
            if (i == insert_pa_)
                new_b.push_back(pickup_a_);
            else if (i == insert_da_)
                new_b.push_back(delivery_a_);
            else if (bi < nb)
                new_b.push_back(reduced_b[bi++]);
        }
    }

    // Clear both routes first, then assign.
    sol.set_route_clients(route_a_, {});
    sol.set_route_clients(route_b_, {});
    sol.set_route_clients(route_a_, std::move(new_a));
    sol.set_route_clients(route_b_, std::move(new_b));
}

} // namespace coso
