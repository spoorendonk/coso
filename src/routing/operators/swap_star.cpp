#include "routing/operators/swap_star.h"

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

/// Evaluate the cost of a route after removing client at remove_pos and
/// inserting insert_client at insert_pos (in the reduced sequence).
inline int64_t eval_route_with_swap(
    Route const& route,
    CostEvaluator const& eval,
    ProblemData const& data,
    int remove_pos,
    int insert_client,
    int insert_pos)
{
    std::vector<int> clients;
    clients.reserve(route.size());
    for (int i = 0; i < route.size(); ++i)
        if (i != remove_pos)
            clients.push_back(route.client(i));
    clients.insert(clients.begin() + insert_pos, insert_client);

    Route temp(data, route.vehicle_type());
    temp.set_clients(std::move(clients));
    return eval.route_cost(temp);
}

/// Evaluate the cost of a route after removing the client at the given pos.
inline int64_t eval_route_after_remove(
    Route const& route,
    CostEvaluator const& eval,
    ProblemData const& data,
    int remove_pos)
{
    std::vector<int> clients;
    clients.reserve(route.size() - 1);
    for (int i = 0; i < route.size(); ++i)
        if (i != remove_pos)
            clients.push_back(route.client(i));

    Route temp(data, route.vehicle_type());
    temp.set_clients(std::move(clients));
    return eval.route_cost(temp);
}

/// Find the best insertion position for client in a route that has already
/// had one client removed at remove_pos.
struct BestInsertResult {
    int pos;
    int64_t cost;  // total route cost after insertion
};

inline BestInsertResult best_insert_after_remove(
    Route const& route,
    CostEvaluator const& eval,
    ProblemData const& data,
    int remove_pos,
    int insert_client)
{
    int reduced_size = route.size() - 1;
    BestInsertResult best{0, std::numeric_limits<int64_t>::max()};

    for (int p = 0; p <= reduced_size; ++p) {
        int64_t cost = eval_route_with_swap(
            route, eval, data, remove_pos, insert_client, p);
        if (cost < best.cost) {
            best.cost = cost;
            best.pos = p;
        }
    }
    return best;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  SwapStar::best_insert
// ---------------------------------------------------------------------------

SwapStar::InsertPos SwapStar::best_insert(Route const& route,
                                           CostEvaluator const& eval,
                                           int client)
{
    InsertPos best;
    best.delta = std::numeric_limits<int64_t>::max();

    for (int p = 0; p <= route.size(); ++p) {
        int64_t d = eval.eval_insert_cost(route, p, client);
        if (d < best.delta) {
            best.delta = d;
            best.pos = p;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
//  SwapStar::find_best_move
// ---------------------------------------------------------------------------

bool SwapStar::find_best_move(Solution const& sol,
                               CostEvaluator const& eval,
                               ProblemData const& data)
{
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);
    int num_routes = sol.num_routes();

    for (int ra = 0; ra < num_routes; ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.empty())
            continue;

        // Determine candidate routes via granular neighbours.
        std::vector<bool> route_seen(num_routes, false);
        route_seen[ra] = true;

        std::vector<int> candidate_routes;

        if (data.granular_k() > 0) {
            for (int pa = 0; pa < route_a.size(); ++pa) {
                int u = route_a.client(pa);
                auto nbrs = data.neighbours(u);
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
                    candidate_routes.push_back(rb);
                }
            }
        } else {
            for (int rb = 0; rb < num_routes; ++rb) {
                if (rb != ra && !sol.route(rb).empty())
                    candidate_routes.push_back(rb);
            }
        }

        for (int rb : candidate_routes) {
            if (rb <= ra)
                continue;  // avoid double-counting

            auto const& route_b = sol.route(rb);
            if (route_b.empty())
                continue;

            int64_t old_cost_a = eval.route_cost(route_a);
            int64_t old_cost_b = eval.route_cost(route_b);
            int64_t old_total = old_cost_a + old_cost_b;

            // --- SWAP*: for each (u in ra, v in rb), remove u and v,
            //     insert u at best pos in rb and v at best pos in ra.
            for (int pa = 0; pa < route_a.size(); ++pa) {
                int u = route_a.client(pa);

                for (int pb = 0; pb < route_b.size(); ++pb) {
                    int v = route_b.client(pb);

                    auto best_v = best_insert_after_remove(
                        route_a, eval, data, pa, v);
                    auto best_u = best_insert_after_remove(
                        route_b, eval, data, pb, u);

                    int64_t new_total = best_v.cost + best_u.cost;
                    int64_t delta = new_total - old_total;

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        move_type_ = kSwap;
                        route_a_ = ra;
                        route_b_ = rb;
                        client_u_ = u;
                        client_v_ = v;
                        pos_u_ = pa;
                        pos_v_ = pb;
                        insert_u_ = best_u.pos;
                        insert_v_ = best_v.pos;
                    }
                }
            }

            // --- Degenerate case 1: relocate u from ra to rb.
            for (int pa = 0; pa < route_a.size(); ++pa) {
                int u = route_a.client(pa);
                int64_t cost_a_after = eval_route_after_remove(
                    route_a, eval, data, pa);
                auto ins = best_insert(route_b, eval, u);
                int64_t new_total = cost_a_after + old_cost_b + ins.delta;
                int64_t delta = new_total - old_total;

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = kRelocateAtoB;
                    route_a_ = ra;
                    route_b_ = rb;
                    client_u_ = u;
                    client_v_ = -1;
                    pos_u_ = pa;
                    pos_v_ = -1;
                    insert_u_ = ins.pos;
                    insert_v_ = -1;
                }
            }

            // --- Degenerate case 2: relocate v from rb to ra.
            for (int pb = 0; pb < route_b.size(); ++pb) {
                int v = route_b.client(pb);
                int64_t cost_b_after = eval_route_after_remove(
                    route_b, eval, data, pb);
                auto ins = best_insert(route_a, eval, v);
                int64_t new_total = old_cost_a + ins.delta + cost_b_after;
                int64_t delta = new_total - old_total;

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = kRelocateBtoA;
                    route_a_ = ra;
                    route_b_ = rb;
                    client_u_ = -1;
                    client_v_ = v;
                    pos_u_ = -1;
                    pos_v_ = pb;
                    insert_u_ = -1;
                    insert_v_ = ins.pos;
                }
            }
        }
    }

    return best_delta_ < 0;
}

// ---------------------------------------------------------------------------
//  SwapStar::apply
// ---------------------------------------------------------------------------

void SwapStar::apply(Solution& sol) const
{
    assert(route_a_ >= 0 && route_b_ >= 0);

    if (move_type_ == kSwap) {
        auto const& ra = sol.route(route_a_);
        auto const& rb = sol.route(route_b_);

        // Route A: remove u at pos_u_, insert v at insert_v_.
        std::vector<int> ca;
        ca.reserve(ra.size());
        for (int i = 0; i < ra.size(); ++i)
            if (i != pos_u_)
                ca.push_back(ra.client(i));
        ca.insert(ca.begin() + insert_v_, client_v_);

        // Route B: remove v at pos_v_, insert u at insert_u_.
        std::vector<int> cb;
        cb.reserve(rb.size());
        for (int i = 0; i < rb.size(); ++i)
            if (i != pos_v_)
                cb.push_back(rb.client(i));
        cb.insert(cb.begin() + insert_u_, client_u_);

        // Clear both routes then set new sequences.
        sol.set_route_clients(route_a_, {});
        sol.set_route_clients(route_b_, {});
        sol.set_route_clients(route_a_, std::move(ca));
        sol.set_route_clients(route_b_, std::move(cb));

    } else if (move_type_ == kRelocateAtoB) {
        sol.remove_client(route_a_, pos_u_);
        sol.insert_client(route_b_, insert_u_, client_u_);

    } else {
        assert(move_type_ == kRelocateBtoA);
        sol.remove_client(route_b_, pos_v_);
        sol.insert_client(route_a_, insert_v_, client_v_);
    }
}

} // namespace coso
