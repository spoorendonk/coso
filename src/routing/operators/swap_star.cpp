#include "routing/operators/swap_star.h"

#include "routing/resources/load_resource.h"
#include "search/stop_criterion.h"

#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace coso {

namespace {

constexpr Cost kInf = kCostMax;

/// Location of a client in the solution: (route index, position in route).
struct ClientLocation {
    int route;
    int pos;
};

/// Build a lookup table: client -> (route, position).
std::vector<ClientLocation> build_client_locations(Solution const& sol) {
    int nc = sol.data().num_clients();
    std::vector<ClientLocation> loc(nc, {-1, -1});
    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);
        for (int p = 0; p < route.size(); ++p) {
            loc[route.client(p)] = {r, p};
        }
    }
    return loc;
}

/// The three cheapest insertion positions of one client in one route
/// (Vidal 2022, §4).  `delta` is the full penalised insertion delta;
/// `free` leaves out its load-penalty part, which SWAP* prices separately.
/// The entries are ranked by `free`; `best_full` is the cheapest position by
/// full delta, which differs from e[0] only when pickups make the load
/// penalty depend on the position.  Relocations use `best_full`.
struct Top3 {
    struct Entry {
        int pos = -1;
        Cost delta = kInf;
        Cost free = kInf;
    };
    std::array<Entry, 3> e;
    Entry best_full;

    void add(int pos, Cost delta, Cost free) {
        if (delta < best_full.delta) {
            best_full = {pos, delta, free};
        }
        if (free >= e[2].free) {
            return;
        }
        e[2] = {pos, delta, free};
        if (e[2].free < e[1].free) {
            std::swap(e[2], e[1]);
            if (e[1].free < e[0].free) {
                std::swap(e[1], e[0]);
            }
        }
    }
};

/// Load-penalty delta of a route after replacing the client at pos with
/// `client` in place.  For delivery-only loads this is the load-penalty delta
/// of removing that client and inserting `client` anywhere.
Cost replace_load_penalty(Route const& route, int pos, int client, Cost lp) {
    auto const& data = route.data();
    auto merged = LoadResource::merge(route.load_prefix(pos - 1), LoadResource::init(data, client));
    merged = LoadResource::merge(merged, route.load_suffix(pos + 1));
    int excess = LoadResource::excess(merged, data.vehicle_type(route.vehicle_type()));
    return static_cast<Cost>(excess - route.load_excess()) * lp;
}

/// Exact penalised delta of a route after removing the client at remove_pos
/// and inserting `client` at position `insert_pos` of the reduced sequence.
Cost exact_swap_delta(Route const& route, CostEvaluator const& eval, int remove_pos, int client,
                      int insert_pos, std::vector<int>& seq) {
    seq.clear();
    auto clients = route.clients();
    if (insert_pos <= remove_pos) {
        // [.. q-1] client [q .. pa-1] [pa+1 ..]
        seq.push_back(client);
        seq.insert(seq.end(), clients.begin() + insert_pos, clients.begin() + remove_pos);
        return eval.eval_splice_cost(route, insert_pos, remove_pos + 1, seq);
    }
    // [.. pa-1] [pa+1 .. q] client [q+1 ..]
    seq.insert(seq.end(), clients.begin() + remove_pos + 1, clients.begin() + insert_pos + 1);
    seq.push_back(client);
    return eval.eval_splice_cost(route, remove_pos, insert_pos + 1, seq);
}

/// Cheapest load-free insertion of a client into a route that has lost the
/// client at remove_pos, choosing among the cached top-3 positions not
/// adjacent to the removed client and the removed client's own place.
/// Returns (free delta, position in the reduced sequence).
std::pair<Cost, int> best_insert_without(Top3 const& top, Cost inplace_free, int remove_pos) {
    Cost best = inplace_free;
    int best_pos = remove_pos;
    for (auto const& entry : top.e) {
        if (entry.pos < 0) {
            break;
        }
        if (entry.pos == remove_pos || entry.pos == remove_pos + 1) {
            continue;  // this insertion point has the removed client as a neighbour
        }
        if (entry.free < best) {
            best = entry.free;
            best_pos = entry.pos < remove_pos ? entry.pos : entry.pos - 1;
        }
        break;  // entries are sorted: the first valid one is the best of them
    }
    return {best, best_pos};
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  SwapStar::find_best_move
// ---------------------------------------------------------------------------

bool SwapStar::find_best_move(Solution const& sol, CostEvaluator const& eval,
                              ProblemData const& data, StopCriterion const* stop) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);
    int num_routes = sol.num_routes();
    Cost lp = eval.load_penalty();

    std::vector<bool> route_seen(num_routes);
    std::vector<int> candidate_routes;
    std::vector<Cost> rem_a, rem_b, rem_free_a, rem_free_b;
    std::vector<Top3> top_u_in_b, top_v_in_a;
    std::vector<int> seq;

    // Removal deltas of every client of a route: full, and load-free.
    auto removal_costs = [&](Route const& route, std::vector<Cost>& full, std::vector<Cost>& free) {
        full.resize(route.size());
        free.resize(route.size());
        for (int p = 0; p < route.size(); ++p) {
            full[p] = eval.eval_remove_cost(route, p);
            free[p] =
                full[p] - static_cast<Cost>(route.eval_remove_load(p) - route.load_excess()) * lp;
        }
    };

    // Top-3 insertion positions in `target` of every client of `source`.
    auto insertion_costs = [&](Route const& source, Route const& target, std::vector<Top3>& tops) {
        tops.assign(source.size(), Top3{});
        for (int i = 0; i < source.size(); ++i) {
            int c = source.client(i);
            for (int p = 0; p <= target.size(); ++p) {
                Cost d = eval.eval_insert_cost(target, p, c);
                Cost load =
                    static_cast<Cost>(target.eval_insert_load(p, c) - target.load_excess()) * lp;
                tops[i].add(p, d, d - load);
            }
        }
    };

    for (int ra = 0; ra < num_routes; ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.empty()) {
            continue;
        }

        // Determine candidate routes via granular neighbours.
        std::fill(route_seen.begin(), route_seen.end(), false);
        route_seen[ra] = true;
        candidate_routes.clear();

        if (data.granular_k() > 0) {
            for (int pa = 0; pa < route_a.size(); ++pa) {
                int u = route_a.client(pa);
                for (int nb_node : data.neighbours(u)) {
                    if (nb_node < data.num_depots()) {
                        continue;
                    }
                    int rb = locations[nb_node - data.num_depots()].route;
                    if (rb < 0 || route_seen[rb]) {
                        continue;
                    }
                    route_seen[rb] = true;
                    candidate_routes.push_back(rb);
                }
            }
        } else {
            for (int rb = 0; rb < num_routes; ++rb) {
                if (rb != ra && !sol.route(rb).empty()) {
                    candidate_routes.push_back(rb);
                }
            }
        }

        bool removal_a_done = false;

        for (int rb : candidate_routes) {
            if (rb <= ra) {
                continue;  // avoid double-counting
            }
            auto const& route_b = sol.route(rb);
            if (route_b.empty()) {
                continue;
            }

            // A pair costs O(|A||B|(|A|+|B|)); stop between pairs, keeping the
            // best exact move found so far, which is still a valid one.
            if (stop && stop->should_stop()) {
                return is_improving(best_delta_);
            }

            if (!removal_a_done) {
                removal_costs(route_a, rem_a, rem_free_a);
                removal_a_done = true;
            }
            removal_costs(route_b, rem_b, rem_free_b);
            insertion_costs(route_a, route_b, top_u_in_b);
            insertion_costs(route_b, route_a, top_v_in_a);

            // --- SWAP*: for each (u in A, v in B), estimate the delta of
            //     removing both and inserting each at its best position in the
            //     other route, from the cached removal and top-3 insertion
            //     costs plus the exact load penalty of the exchange.
            Cost best_est = 0;
            int best_pa = -1;
            int best_pb = -1;
            int best_ins_u = -1;
            int best_ins_v = -1;

            for (int pa = 0; pa < route_a.size(); ++pa) {
                int u = route_a.client(pa);
                for (int pb = 0; pb < route_b.size(); ++pb) {
                    int v = route_b.client(pb);

                    Cost load_a = replace_load_penalty(route_a, pa, v, lp);
                    Cost load_b = replace_load_penalty(route_b, pb, u, lp);

                    // Cost of putting v exactly where u was, without load.
                    Cost inplace_v =
                        eval.eval_splice_cost(route_a, pa, pa + 1, std::span<int const>(&v, 1)) -
                        load_a - rem_free_a[pa];
                    Cost inplace_u =
                        eval.eval_splice_cost(route_b, pb, pb + 1, std::span<int const>(&u, 1)) -
                        load_b - rem_free_b[pb];

                    auto [ins_v, pos_v] = best_insert_without(top_v_in_a[pb], inplace_v, pa);
                    auto [ins_u, pos_u] = best_insert_without(top_u_in_b[pa], inplace_u, pb);

                    Cost est = rem_free_a[pa] + ins_v + load_a + rem_free_b[pb] + ins_u + load_b;
                    if (est < best_est) {
                        best_est = est;
                        best_pa = pa;
                        best_pb = pb;
                        best_ins_u = pos_u;
                        best_ins_v = pos_v;
                    }
                }
            }

            // The estimate approximates the interaction of the two changes in
            // one route; price the chosen swap exactly before keeping it.
            if (best_pa >= 0) {
                int u = route_a.client(best_pa);
                int v = route_b.client(best_pb);
                Cost delta = exact_swap_delta(route_a, eval, best_pa, v, best_ins_v, seq) +
                             exact_swap_delta(route_b, eval, best_pb, u, best_ins_u, seq);
                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = Swap;
                    route_a_ = ra;
                    route_b_ = rb;
                    client_u_ = u;
                    client_v_ = v;
                    pos_u_ = best_pa;
                    pos_v_ = best_pb;
                    insert_u_ = best_ins_u;
                    insert_v_ = best_ins_v;
                }
            }

            // --- Degenerate case 1: relocate u from A to its best place in B.
            //     The two routes change independently, so this delta is exact.
            for (int pa = 0; pa < route_a.size(); ++pa) {
                auto const& best = top_u_in_b[pa].best_full;
                Cost delta = rem_a[pa] + best.delta;
                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = RelocateAtoB;
                    route_a_ = ra;
                    route_b_ = rb;
                    client_u_ = route_a.client(pa);
                    client_v_ = -1;
                    pos_u_ = pa;
                    pos_v_ = -1;
                    insert_u_ = best.pos;
                    insert_v_ = -1;
                }
            }

            // --- Degenerate case 2: relocate v from B to its best place in A.
            for (int pb = 0; pb < route_b.size(); ++pb) {
                auto const& best = top_v_in_a[pb].best_full;
                Cost delta = rem_b[pb] + best.delta;
                if (delta < best_delta_) {
                    best_delta_ = delta;
                    move_type_ = RelocateBtoA;
                    route_a_ = ra;
                    route_b_ = rb;
                    client_u_ = -1;
                    client_v_ = route_b.client(pb);
                    pos_u_ = -1;
                    pos_v_ = pb;
                    insert_u_ = -1;
                    insert_v_ = best.pos;
                }
            }
        }
    }

    return is_improving(best_delta_);
}

// ---------------------------------------------------------------------------
//  SwapStar::apply
// ---------------------------------------------------------------------------

void SwapStar::apply(Solution& sol) const {
    assert(route_a_ >= 0 && route_b_ >= 0);

    if (move_type_ == Swap) {
        auto const& ra = sol.route(route_a_);
        auto const& rb = sol.route(route_b_);

        // Route A: remove u at pos_u_, insert v at insert_v_.
        std::vector<int> ca;
        ca.reserve(ra.size());
        for (int i = 0; i < ra.size(); ++i) {
            if (i != pos_u_) {
                ca.push_back(ra.client(i));
            }
        }
        ca.insert(ca.begin() + insert_v_, client_v_);

        // Route B: remove v at pos_v_, insert u at insert_u_.
        std::vector<int> cb;
        cb.reserve(rb.size());
        for (int i = 0; i < rb.size(); ++i) {
            if (i != pos_v_) {
                cb.push_back(rb.client(i));
            }
        }
        cb.insert(cb.begin() + insert_u_, client_u_);

        // Clear both routes then set new sequences.
        sol.set_route_clients(route_a_, {});
        sol.set_route_clients(route_b_, {});
        sol.set_route_clients(route_a_, std::move(ca));
        sol.set_route_clients(route_b_, std::move(cb));

    } else if (move_type_ == RelocateAtoB) {
        sol.remove_client(route_a_, pos_u_);
        sol.insert_client(route_b_, insert_u_, client_u_);

    } else {
        assert(move_type_ == RelocateBtoA);
        sol.remove_client(route_b_, pos_v_);
        sol.insert_client(route_a_, insert_v_, client_v_);
    }
}

}  // namespace coso
