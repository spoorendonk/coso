#include "routing/operators/exchange.h"

#include <cassert>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

namespace {

/// Convert client index (0-based among clients) to node index.
inline int node_of(ProblemData const& data, int client) {
    return data.num_depots() + client;
}

/// Location of a client in the solution: (route index, position in route).
struct ClientLocation {
    int route;
    int pos;
};

/// Build a lookup table: client -> (route, position).
inline std::vector<ClientLocation> build_client_locations(Solution const& sol) {
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

// ---- O(1) evaluation helpers for segments and replacements ----

/// Distance delta for removing two consecutive clients at pos, pos+1.
inline int eval_remove_pair_dist(Route const& route, ProblemData const& data, int pos) {
    int profile = data.vehicle_type(route.vehicle_type()).profile;
    int nd = data.num_depots();
    int prev = (pos == 0) ? 0 : nd + route.client(pos - 1);
    int u = nd + route.client(pos);
    int v = nd + route.client(pos + 1);
    int next = (pos + 1 == route.size() - 1) ? 0 : nd + route.client(pos + 2);
    return data.dist(profile, prev, next) - data.dist(profile, prev, u) - data.dist(profile, u, v) -
           data.dist(profile, v, next);
}

/// Load excess after removing two consecutive clients at pos, pos+1.
inline int eval_remove_pair_load(Route const& route, ProblemData const& data, int pos) {
    auto const& left = route.load_prefix(pos - 1);
    auto const& right = route.load_suffix(pos + 2);
    auto merged = LoadResource::merge(left, right);
    return LoadResource::excess(merged, data.vehicle_type(route.vehicle_type()));
}

/// Penalized cost delta for removing pair at pos.
inline int64_t eval_remove_pair_cost(Route const& route, CostEvaluator const& eval,
                                     ProblemData const& data, int pos) {
    auto const& vt = data.vehicle_type(route.vehicle_type());
    int64_t delta = 0;
    delta +=
        static_cast<int64_t>(eval_remove_pair_dist(route, data, pos)) * vt.cost.unit_distance_cost;
    if (route.size() == 2) {
        delta -= vt.cost.fixed_cost;
    }
    delta += data.client(route.client(pos)).prize;
    delta += data.client(route.client(pos + 1)).prize;
    int new_excess = eval_remove_pair_load(route, data, pos);
    delta += static_cast<int64_t>(new_excess - route.load_excess()) * eval.load_penalty();
    return delta;
}

/// Distance delta for inserting clients c1, c2 consecutively at pos.
inline int eval_insert_pair_dist(Route const& route, ProblemData const& data, int pos, int c1,
                                 int c2) {
    int profile = data.vehicle_type(route.vehicle_type()).profile;
    int nd = data.num_depots();
    int prev = (pos == 0) ? 0 : nd + route.client(pos - 1);
    int next = (pos == route.size()) ? 0 : nd + route.client(pos);
    int u = nd + c1;
    int v = nd + c2;
    return data.dist(profile, prev, u) + data.dist(profile, u, v) + data.dist(profile, v, next) -
           data.dist(profile, prev, next);
}

/// Load excess after inserting c1, c2 consecutively at pos.
inline int eval_insert_pair_load(Route const& route, ProblemData const& data, int pos, int c1,
                                 int c2) {
    auto s1 = LoadResource::init(data, c1);
    auto s2 = LoadResource::init(data, c2);
    auto pair = LoadResource::merge(s1, s2);
    auto const& left = route.load_prefix(pos - 1);
    auto const& right = route.load_suffix(pos);
    auto merged = LoadResource::merge(LoadResource::merge(left, pair), right);
    return LoadResource::excess(merged, data.vehicle_type(route.vehicle_type()));
}

/// Penalized cost delta for inserting pair at pos.
inline int64_t eval_insert_pair_cost(Route const& route, CostEvaluator const& eval,
                                     ProblemData const& data, int pos, int c1, int c2) {
    auto const& vt = data.vehicle_type(route.vehicle_type());
    int64_t delta = 0;
    delta += static_cast<int64_t>(eval_insert_pair_dist(route, data, pos, c1, c2)) *
             vt.cost.unit_distance_cost;
    if (route.empty()) {
        delta += vt.cost.fixed_cost;
    }
    delta -= data.client(c1).prize;
    delta -= data.client(c2).prize;
    int new_excess = eval_insert_pair_load(route, data, pos, c1, c2);
    delta += static_cast<int64_t>(new_excess - route.load_excess()) * eval.load_penalty();
    return delta;
}

/// Distance delta for replacing client at pos with new_client.
inline int eval_replace_dist(Route const& route, ProblemData const& data, int pos, int new_client) {
    int profile = data.vehicle_type(route.vehicle_type()).profile;
    int nd = data.num_depots();
    int old_n = nd + route.client(pos);
    int new_n = nd + new_client;
    int prev = (pos == 0) ? 0 : nd + route.client(pos - 1);
    int next = (pos == route.size() - 1) ? 0 : nd + route.client(pos + 1);
    return data.dist(profile, prev, new_n) + data.dist(profile, new_n, next) -
           data.dist(profile, prev, old_n) - data.dist(profile, old_n, next);
}

/// Load excess after replacing client at pos with new_client.
inline int eval_replace_load(Route const& route, ProblemData const& data, int pos, int new_client) {
    auto const& left = route.load_prefix(pos - 1);
    auto ns = LoadResource::init(data, new_client);
    auto const& right = route.load_suffix(pos + 1);
    auto merged = LoadResource::merge(LoadResource::merge(left, ns), right);
    return LoadResource::excess(merged, data.vehicle_type(route.vehicle_type()));
}

/// Penalized cost delta for replacing client at pos with new_client.
inline int64_t eval_replace_cost(Route const& route, CostEvaluator const& eval,
                                 ProblemData const& data, int pos, int new_client) {
    auto const& vt = data.vehicle_type(route.vehicle_type());
    int64_t delta = 0;
    delta += static_cast<int64_t>(eval_replace_dist(route, data, pos, new_client)) *
             vt.cost.unit_distance_cost;
    delta += data.client(route.client(pos)).prize;
    delta -= data.client(new_client).prize;
    int new_excess = eval_replace_load(route, data, pos, new_client);
    delta += static_cast<int64_t>(new_excess - route.load_excess()) * eval.load_penalty();
    return delta;
}

/// Evaluate intra-route relocate: remove client at from_pos, insert at
/// to_pos (in the reduced sequence after removal).  Uses a temporary Route
/// to get exact cost.
inline int64_t eval_intra_relocate(Route const& route, CostEvaluator const& eval,
                                   ProblemData const& data, int from_pos, int to_pos) {
    int client = route.client(from_pos);
    std::vector<int> clients;
    clients.reserve(route.size());
    for (int i = 0; i < route.size(); ++i) {
        if (i != from_pos) {
            clients.push_back(route.client(i));
        }
    }
    clients.insert(clients.begin() + to_pos, client);

    Route temp(data, route.vehicle_type());
    temp.set_clients(std::move(clients));
    return eval.route_cost(temp) - eval.route_cost(route);
}

}  // anonymous namespace

// ===========================================================================
//  Exchange(1,0) -- Relocate
// ===========================================================================

bool Exchange10::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    from_route_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);

        for (int pa = 0; pa < route_a.size(); ++pa) {
            int u = route_a.client(pa);
            int64_t remove_delta = eval.eval_remove_cost(route_a, pa);

            // Lambda to evaluate inserting into route rb.
            auto try_route = [&](int rb) {
                auto const& route_b = sol.route(rb);

                if (rb == ra) {
                    // Intra-route: positions in reduced sequence (size-1).
                    int reduced = route_a.size() - 1;
                    for (int pb = 0; pb <= reduced; ++pb) {
                        // Skip trivial moves (reinsert at same effective spot).
                        // In the reduced sequence (without pa), position pb
                        // corresponds to original positions shifted around pa.
                        int orig = (pb >= pa) ? pb + 1 : pb;
                        // Reinserting before or after original position is no-op.
                        if (orig == pa || orig == pa + 1) {
                            continue;
                        }

                        int64_t delta = eval_intra_relocate(route_a, eval, data, pa, pb);

                        if (delta < best_delta_) {
                            best_delta_ = delta;
                            from_route_ = ra;
                            from_pos_ = pa;
                            to_route_ = rb;
                            to_pos_ = pb;
                            client_ = u;
                        }
                    }
                } else {
                    // Inter-route: remove from A + insert into B.
                    for (int pb = 0; pb <= route_b.size(); ++pb) {
                        int64_t insert_delta = eval.eval_insert_cost(route_b, pb, u);
                        int64_t delta = remove_delta + insert_delta;

                        if (delta < best_delta_) {
                            best_delta_ = delta;
                            from_route_ = ra;
                            from_pos_ = pa;
                            to_route_ = rb;
                            to_pos_ = pb;
                            client_ = u;
                        }
                    }
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                auto nbrs = data.neighbours(u);
                for (int nb_node : nbrs) {
                    if (nb_node < data.num_depots()) {
                        continue;
                    }
                    int nb_client = nb_node - data.num_depots();
                    auto const& loc = locations[nb_client];
                    if (loc.route < 0) {
                        continue;
                    }
                    int rb = loc.route;
                    if (route_seen[rb]) {
                        continue;
                    }
                    route_seen[rb] = true;
                    try_route(rb);
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    try_route(rb);
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange10::apply(Solution& sol) const {
    assert(from_route_ >= 0);

    if (from_route_ == to_route_) {
        // Intra-route: build new sequence.
        auto const& route = sol.route(from_route_);
        std::vector<int> clients;
        clients.reserve(route.size());
        for (int i = 0; i < route.size(); ++i) {
            if (i != from_pos_) {
                clients.push_back(route.client(i));
            }
        }
        clients.insert(clients.begin() + to_pos_, client_);
        sol.set_route_clients(from_route_, std::move(clients));
    } else {
        sol.remove_client(from_route_, from_pos_);
        sol.insert_client(to_route_, to_pos_, client_);
    }
}

// ===========================================================================
//  Exchange(1,1) -- Swap
// ===========================================================================

bool Exchange11::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);

        for (int pa = 0; pa < route_a.size(); ++pa) {
            int u = route_a.client(pa);

            auto try_swap = [&](int rb, int pb) {
                // Avoid double-counting.
                if (rb < ra || (rb == ra && pb <= pa)) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                int v = route_b.client(pb);

                int64_t delta;
                if (ra == rb) {
                    // Intra-route swap: build temporary route.
                    std::vector<int> new_clients;
                    new_clients.reserve(route_a.size());
                    for (int i = 0; i < route_a.size(); ++i) {
                        if (i == pa) {
                            new_clients.push_back(v);
                        } else if (i == pb) {
                            new_clients.push_back(u);
                        } else {
                            new_clients.push_back(route_a.client(i));
                        }
                    }
                    Route temp(data, route_a.vehicle_type());
                    temp.set_clients(std::move(new_clients));
                    delta = eval.route_cost(temp) - eval.route_cost(route_a);
                } else {
                    // Inter-route: independent replacements.
                    delta = eval_replace_cost(route_a, eval, data, pa, v) +
                            eval_replace_cost(route_b, eval, data, pb, u);
                }

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    route_a_ = ra;
                    pos_a_ = pa;
                    route_b_ = rb;
                    pos_b_ = pb;
                }
            };

            if (data.granular_k() > 0) {
                auto nbrs = data.neighbours(u);
                for (int nb_node : nbrs) {
                    if (nb_node < data.num_depots()) {
                        continue;
                    }
                    int nb_client = nb_node - data.num_depots();
                    auto const& loc = locations[nb_client];
                    if (loc.route < 0) {
                        continue;
                    }
                    try_swap(loc.route, loc.pos);
                }
            } else {
                for (int rb = ra; rb < sol.num_routes(); ++rb) {
                    auto const& route_b = sol.route(rb);
                    int start = (rb == ra) ? pa + 1 : 0;
                    for (int pb = start; pb < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange11::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    int u = sol.route(route_a_).client(pos_a_);
    int v = sol.route(route_b_).client(pos_b_);

    if (route_a_ == route_b_) {
        auto const& route = sol.route(route_a_);
        std::vector<int> clients;
        clients.reserve(route.size());
        for (int i = 0; i < route.size(); ++i) {
            if (i == pos_a_) {
                clients.push_back(v);
            } else if (i == pos_b_) {
                clients.push_back(u);
            } else {
                clients.push_back(route.client(i));
            }
        }
        sol.set_route_clients(route_a_, std::move(clients));
    } else {
        // Inter-route swap: build both new client lists first, then clear
        // and set both routes to avoid assignment tracking conflicts.
        // (set_route_clients unmarks old clients, which can conflict when
        // swapped clients appear in both old routes.)
        auto const& ra = sol.route(route_a_);
        auto const& rb = sol.route(route_b_);

        std::vector<int> ca, cb;
        ca.reserve(ra.size());
        cb.reserve(rb.size());
        for (int i = 0; i < ra.size(); ++i) {
            ca.push_back(i == pos_a_ ? v : ra.client(i));
        }
        for (int i = 0; i < rb.size(); ++i) {
            cb.push_back(i == pos_b_ ? u : rb.client(i));
        }

        // Clear both routes, then assign new sequences.
        sol.set_route_clients(route_a_, {});
        sol.set_route_clients(route_b_, {});
        sol.set_route_clients(route_a_, std::move(ca));
        sol.set_route_clients(route_b_, std::move(cb));
    }
}

// ===========================================================================
//  Exchange(2,0) -- Relocate pair
// ===========================================================================

bool Exchange20::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    from_route_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 2) {
            continue;
        }

        for (int pa = 0; pa + 1 < route_a.size(); ++pa) {
            int c1 = route_a.client(pa);
            int c2 = route_a.client(pa + 1);
            int64_t remove_delta = eval_remove_pair_cost(route_a, eval, data, pa);

            auto try_route = [&](int rb) {
                if (rb == ra) {
                    return;  // inter-route only
                }
                auto const& route_b = sol.route(rb);
                for (int pb = 0; pb <= route_b.size(); ++pb) {
                    int64_t insert_delta = eval_insert_pair_cost(route_b, eval, data, pb, c1, c2);
                    int64_t delta = remove_delta + insert_delta;
                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        from_route_ = ra;
                        from_pos_ = pa;
                        to_route_ = rb;
                        to_pos_ = pb;
                    }
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;
                for (int c : {c1, c2}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0) {
                            continue;
                        }
                        int rb = loc.route;
                        if (route_seen[rb]) {
                            continue;
                        }
                        route_seen[rb] = true;
                        try_route(rb);
                    }
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    try_route(rb);
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange20::apply(Solution& sol) const {
    assert(from_route_ >= 0);

    int c1 = sol.route(from_route_).client(from_pos_);
    int c2 = sol.route(from_route_).client(from_pos_ + 1);

    // Remove from source (second first to keep positions valid).
    sol.remove_client(from_route_, from_pos_ + 1);
    sol.remove_client(from_route_, from_pos_);

    // Insert into target.
    sol.insert_client(to_route_, to_pos_, c1);
    sol.insert_client(to_route_, to_pos_ + 1, c2);
}

// ===========================================================================
//  Exchange(2,1) -- Swap pair with single
// ===========================================================================

bool Exchange21::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 2) {
            continue;
        }

        for (int pa = 0; pa + 1 < route_a.size(); ++pa) {
            int u1 = route_a.client(pa);
            int u2 = route_a.client(pa + 1);

            auto try_swap = [&](int rb, int pb) {
                if (rb == ra) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                int v = route_b.client(pb);

                // Build new route A: replace pair at pa,pa+1 with single v.
                std::vector<int> new_a;
                new_a.reserve(route_a.size() - 1);
                for (int i = 0; i < route_a.size(); ++i) {
                    if (i == pa) {
                        new_a.push_back(v);
                    } else if (i == pa + 1) {
                        continue;
                    } else {
                        new_a.push_back(route_a.client(i));
                    }
                }

                // Build new route B: replace single at pb with pair u1,u2.
                std::vector<int> new_b;
                new_b.reserve(route_b.size() + 1);
                for (int i = 0; i < route_b.size(); ++i) {
                    if (i == pb) {
                        new_b.push_back(u1);
                        new_b.push_back(u2);
                    } else {
                        new_b.push_back(route_b.client(i));
                    }
                }

                Route temp_a(data, route_a.vehicle_type());
                temp_a.set_clients(std::move(new_a));
                Route temp_b(data, route_b.vehicle_type());
                temp_b.set_clients(std::move(new_b));

                int64_t delta = eval.route_cost(temp_a) + eval.route_cost(temp_b) -
                                eval.route_cost(route_a) - eval.route_cost(route_b);

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    route_a_ = ra;
                    pos_a_ = pa;
                    route_b_ = rb;
                    pos_b_ = pb;
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;
                for (int c : {u1, u2}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        auto const& rb_route = sol.route(loc.route);
                        for (int pb = 0; pb < rb_route.size(); ++pb) {
                            try_swap(loc.route, pb);
                        }
                    }
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    if (rb == ra) {
                        continue;
                    }
                    auto const& route_b = sol.route(rb);
                    for (int pb = 0; pb < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange21::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& ra = sol.route(route_a_);
    auto const& rb = sol.route(route_b_);

    int u1 = ra.client(pos_a_);
    int u2 = ra.client(pos_a_ + 1);
    int v = rb.client(pos_b_);

    std::vector<int> new_a;
    new_a.reserve(ra.size() - 1);
    for (int i = 0; i < ra.size(); ++i) {
        if (i == pos_a_) {
            new_a.push_back(v);
        } else if (i == pos_a_ + 1) {
            continue;
        } else {
            new_a.push_back(ra.client(i));
        }
    }

    std::vector<int> new_b;
    new_b.reserve(rb.size() + 1);
    for (int i = 0; i < rb.size(); ++i) {
        if (i == pos_b_) {
            new_b.push_back(u1);
            new_b.push_back(u2);
        } else {
            new_b.push_back(rb.client(i));
        }
    }

    sol.set_route_clients(route_a_, {});
    sol.set_route_clients(route_b_, {});
    sol.set_route_clients(route_a_, std::move(new_a));
    sol.set_route_clients(route_b_, std::move(new_b));
}

// ===========================================================================
//  Exchange(2,2) -- Swap pair with pair
// ===========================================================================

bool Exchange22::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 2) {
            continue;
        }

        for (int pa = 0; pa + 1 < route_a.size(); ++pa) {
            int u1 = route_a.client(pa);
            int u2 = route_a.client(pa + 1);

            auto try_swap = [&](int rb, int pb) {
                // Avoid double-counting: only consider rb > ra,
                // or rb == ra with pb > pa+1 (non-overlapping intra-route).
                if (rb < ra || (rb == ra && pb <= pa + 1)) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                if (pb + 1 >= route_b.size()) {
                    return;
                }

                int v1 = route_b.client(pb);
                int v2 = route_b.client(pb + 1);

                if (ra == rb) {
                    // Intra-route: build single new route.
                    std::vector<int> new_clients;
                    new_clients.reserve(route_a.size());
                    for (int i = 0; i < route_a.size(); ++i) {
                        if (i == pa) {
                            new_clients.push_back(v1);
                        } else if (i == pa + 1) {
                            new_clients.push_back(v2);
                        } else if (i == pb) {
                            new_clients.push_back(u1);
                        } else if (i == pb + 1) {
                            new_clients.push_back(u2);
                        } else {
                            new_clients.push_back(route_a.client(i));
                        }
                    }
                    Route temp(data, route_a.vehicle_type());
                    temp.set_clients(std::move(new_clients));
                    int64_t delta = eval.route_cost(temp) - eval.route_cost(route_a);

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        route_a_ = ra;
                        pos_a_ = pa;
                        route_b_ = rb;
                        pos_b_ = pb;
                    }
                } else {
                    // Inter-route: replace pair in A with pair from B.
                    std::vector<int> new_a;
                    new_a.reserve(route_a.size());
                    for (int i = 0; i < route_a.size(); ++i) {
                        if (i == pa) {
                            new_a.push_back(v1);
                        } else if (i == pa + 1) {
                            new_a.push_back(v2);
                        } else {
                            new_a.push_back(route_a.client(i));
                        }
                    }
                    std::vector<int> new_b;
                    new_b.reserve(route_b.size());
                    for (int i = 0; i < route_b.size(); ++i) {
                        if (i == pb) {
                            new_b.push_back(u1);
                        } else if (i == pb + 1) {
                            new_b.push_back(u2);
                        } else {
                            new_b.push_back(route_b.client(i));
                        }
                    }

                    Route temp_a(data, route_a.vehicle_type());
                    temp_a.set_clients(std::move(new_a));
                    Route temp_b(data, route_b.vehicle_type());
                    temp_b.set_clients(std::move(new_b));

                    int64_t delta = eval.route_cost(temp_a) + eval.route_cost(temp_b) -
                                    eval.route_cost(route_a) - eval.route_cost(route_b);

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        route_a_ = ra;
                        pos_a_ = pa;
                        route_b_ = rb;
                        pos_b_ = pb;
                    }
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                for (int c : {u1, u2}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        auto const& rb_route = sol.route(loc.route);
                        for (int pb = 0; pb + 1 < rb_route.size(); ++pb) {
                            try_swap(loc.route, pb);
                        }
                    }
                }
            } else {
                for (int rb = ra; rb < sol.num_routes(); ++rb) {
                    auto const& route_b = sol.route(rb);
                    int start = (rb == ra) ? pa + 2 : 0;
                    for (int pb = start; pb + 1 < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange22::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& ra = sol.route(route_a_);
    auto const& rb = sol.route(route_b_);

    int u1 = ra.client(pos_a_);
    int u2 = ra.client(pos_a_ + 1);
    int v1 = rb.client(pos_b_);
    int v2 = rb.client(pos_b_ + 1);

    if (route_a_ == route_b_) {
        std::vector<int> new_clients;
        new_clients.reserve(ra.size());
        for (int i = 0; i < ra.size(); ++i) {
            if (i == pos_a_) {
                new_clients.push_back(v1);
            } else if (i == pos_a_ + 1) {
                new_clients.push_back(v2);
            } else if (i == pos_b_) {
                new_clients.push_back(u1);
            } else if (i == pos_b_ + 1) {
                new_clients.push_back(u2);
            } else {
                new_clients.push_back(ra.client(i));
            }
        }
        sol.set_route_clients(route_a_, std::move(new_clients));
    } else {
        std::vector<int> new_a, new_b;
        new_a.reserve(ra.size());
        new_b.reserve(rb.size());
        for (int i = 0; i < ra.size(); ++i) {
            if (i == pos_a_) {
                new_a.push_back(v1);
            } else if (i == pos_a_ + 1) {
                new_a.push_back(v2);
            } else {
                new_a.push_back(ra.client(i));
            }
        }
        for (int i = 0; i < rb.size(); ++i) {
            if (i == pos_b_) {
                new_b.push_back(u1);
            } else if (i == pos_b_ + 1) {
                new_b.push_back(u2);
            } else {
                new_b.push_back(rb.client(i));
            }
        }
        sol.set_route_clients(route_a_, {});
        sol.set_route_clients(route_b_, {});
        sol.set_route_clients(route_a_, std::move(new_a));
        sol.set_route_clients(route_b_, std::move(new_b));
    }
}

// ===========================================================================
//  Exchange(3,0) -- Relocate triple (Or-opt-3)
// ===========================================================================

bool Exchange30::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    from_route_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 3) {
            continue;
        }

        for (int pa = 0; pa + 2 < route_a.size(); ++pa) {
            int c1 = route_a.client(pa);
            int c2 = route_a.client(pa + 1);
            int c3 = route_a.client(pa + 2);

            // Evaluate removal of triple using temporary route.
            std::vector<int> reduced;
            reduced.reserve(route_a.size() - 3);
            for (int i = 0; i < route_a.size(); ++i) {
                if (i < pa || i > pa + 2) {
                    reduced.push_back(route_a.client(i));
                }
            }
            Route temp_reduced(data, route_a.vehicle_type());
            temp_reduced.set_clients(std::move(reduced));
            int64_t remove_delta = eval.route_cost(temp_reduced) - eval.route_cost(route_a);

            auto try_route = [&](int rb) {
                if (rb == ra) {
                    return;
                }
                auto const& route_b = sol.route(rb);
                for (int pb = 0; pb <= route_b.size(); ++pb) {
                    std::vector<int> new_b;
                    new_b.reserve(route_b.size() + 3);
                    for (int i = 0; i < pb; ++i) {
                        new_b.push_back(route_b.client(i));
                    }
                    new_b.push_back(c1);
                    new_b.push_back(c2);
                    new_b.push_back(c3);
                    for (int i = pb; i < route_b.size(); ++i) {
                        new_b.push_back(route_b.client(i));
                    }

                    Route temp_b(data, route_b.vehicle_type());
                    temp_b.set_clients(std::move(new_b));
                    int64_t insert_delta = eval.route_cost(temp_b) - eval.route_cost(route_b);
                    int64_t delta = remove_delta + insert_delta;

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        from_route_ = ra;
                        from_pos_ = pa;
                        to_route_ = rb;
                        to_pos_ = pb;
                    }
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;
                for (int c : {c1, c2, c3}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        try_route(loc.route);
                    }
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    try_route(rb);
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange30::apply(Solution& sol) const {
    assert(from_route_ >= 0);

    int c1 = sol.route(from_route_).client(from_pos_);
    int c2 = sol.route(from_route_).client(from_pos_ + 1);
    int c3 = sol.route(from_route_).client(from_pos_ + 2);

    sol.remove_client(from_route_, from_pos_ + 2);
    sol.remove_client(from_route_, from_pos_ + 1);
    sol.remove_client(from_route_, from_pos_);

    sol.insert_client(to_route_, to_pos_, c1);
    sol.insert_client(to_route_, to_pos_ + 1, c2);
    sol.insert_client(to_route_, to_pos_ + 2, c3);
}

// ===========================================================================
//  Exchange(3,1) -- Swap triple with single
// ===========================================================================

bool Exchange31::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 3) {
            continue;
        }

        for (int pa = 0; pa + 2 < route_a.size(); ++pa) {
            int u1 = route_a.client(pa);
            int u2 = route_a.client(pa + 1);
            int u3 = route_a.client(pa + 2);

            auto try_swap = [&](int rb, int pb) {
                if (rb == ra) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                int v = route_b.client(pb);

                std::vector<int> new_a;
                new_a.reserve(route_a.size() - 2);
                for (int i = 0; i < route_a.size(); ++i) {
                    if (i == pa) {
                        new_a.push_back(v);
                    } else if (i == pa + 1 || i == pa + 2) {
                        continue;
                    } else {
                        new_a.push_back(route_a.client(i));
                    }
                }

                std::vector<int> new_b;
                new_b.reserve(route_b.size() + 2);
                for (int i = 0; i < route_b.size(); ++i) {
                    if (i == pb) {
                        new_b.push_back(u1);
                        new_b.push_back(u2);
                        new_b.push_back(u3);
                    } else {
                        new_b.push_back(route_b.client(i));
                    }
                }

                Route temp_a(data, route_a.vehicle_type());
                temp_a.set_clients(std::move(new_a));
                Route temp_b(data, route_b.vehicle_type());
                temp_b.set_clients(std::move(new_b));

                int64_t delta = eval.route_cost(temp_a) + eval.route_cost(temp_b) -
                                eval.route_cost(route_a) - eval.route_cost(route_b);

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    route_a_ = ra;
                    pos_a_ = pa;
                    route_b_ = rb;
                    pos_b_ = pb;
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;
                for (int c : {u1, u2, u3}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        auto const& rb_route = sol.route(loc.route);
                        for (int pb = 0; pb < rb_route.size(); ++pb) {
                            try_swap(loc.route, pb);
                        }
                    }
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    if (rb == ra) {
                        continue;
                    }
                    auto const& route_b = sol.route(rb);
                    for (int pb = 0; pb < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange31::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& ra = sol.route(route_a_);
    auto const& rb = sol.route(route_b_);

    int u1 = ra.client(pos_a_);
    int u2 = ra.client(pos_a_ + 1);
    int u3 = ra.client(pos_a_ + 2);
    int v = rb.client(pos_b_);

    std::vector<int> new_a;
    new_a.reserve(ra.size() - 2);
    for (int i = 0; i < ra.size(); ++i) {
        if (i == pos_a_) {
            new_a.push_back(v);
        } else if (i == pos_a_ + 1 || i == pos_a_ + 2) {
            continue;
        } else {
            new_a.push_back(ra.client(i));
        }
    }

    std::vector<int> new_b;
    new_b.reserve(rb.size() + 2);
    for (int i = 0; i < rb.size(); ++i) {
        if (i == pos_b_) {
            new_b.push_back(u1);
            new_b.push_back(u2);
            new_b.push_back(u3);
        } else {
            new_b.push_back(rb.client(i));
        }
    }

    sol.set_route_clients(route_a_, {});
    sol.set_route_clients(route_b_, {});
    sol.set_route_clients(route_a_, std::move(new_a));
    sol.set_route_clients(route_b_, std::move(new_b));
}

// ===========================================================================
//  Exchange(3,2) -- Swap triple with pair
// ===========================================================================

bool Exchange32::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 3) {
            continue;
        }

        for (int pa = 0; pa + 2 < route_a.size(); ++pa) {
            int u1 = route_a.client(pa);
            int u2 = route_a.client(pa + 1);
            int u3 = route_a.client(pa + 2);

            auto try_swap = [&](int rb, int pb) {
                if (rb == ra) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                if (pb + 1 >= route_b.size()) {
                    return;
                }

                int v1 = route_b.client(pb);
                int v2 = route_b.client(pb + 1);

                std::vector<int> new_a;
                new_a.reserve(route_a.size() - 1);
                for (int i = 0; i < route_a.size(); ++i) {
                    if (i == pa) {
                        new_a.push_back(v1);
                        new_a.push_back(v2);
                    } else if (i == pa + 1 || i == pa + 2) {
                        continue;
                    } else {
                        new_a.push_back(route_a.client(i));
                    }
                }

                std::vector<int> new_b;
                new_b.reserve(route_b.size() + 1);
                for (int i = 0; i < route_b.size(); ++i) {
                    if (i == pb) {
                        new_b.push_back(u1);
                        new_b.push_back(u2);
                        new_b.push_back(u3);
                    } else if (i == pb + 1) {
                        continue;
                    } else {
                        new_b.push_back(route_b.client(i));
                    }
                }

                Route temp_a(data, route_a.vehicle_type());
                temp_a.set_clients(std::move(new_a));
                Route temp_b(data, route_b.vehicle_type());
                temp_b.set_clients(std::move(new_b));

                int64_t delta = eval.route_cost(temp_a) + eval.route_cost(temp_b) -
                                eval.route_cost(route_a) - eval.route_cost(route_b);

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    route_a_ = ra;
                    pos_a_ = pa;
                    route_b_ = rb;
                    pos_b_ = pb;
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;
                for (int c : {u1, u2, u3}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        auto const& rb_route = sol.route(loc.route);
                        for (int pb = 0; pb + 1 < rb_route.size(); ++pb) {
                            try_swap(loc.route, pb);
                        }
                    }
                }
            } else {
                for (int rb = 0; rb < sol.num_routes(); ++rb) {
                    if (rb == ra) {
                        continue;
                    }
                    auto const& route_b = sol.route(rb);
                    for (int pb = 0; pb + 1 < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange32::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& ra = sol.route(route_a_);
    auto const& rb = sol.route(route_b_);

    int u1 = ra.client(pos_a_);
    int u2 = ra.client(pos_a_ + 1);
    int u3 = ra.client(pos_a_ + 2);
    int v1 = rb.client(pos_b_);
    int v2 = rb.client(pos_b_ + 1);

    std::vector<int> new_a;
    new_a.reserve(ra.size() - 1);
    for (int i = 0; i < ra.size(); ++i) {
        if (i == pos_a_) {
            new_a.push_back(v1);
            new_a.push_back(v2);
        } else if (i == pos_a_ + 1 || i == pos_a_ + 2) {
            continue;
        } else {
            new_a.push_back(ra.client(i));
        }
    }

    std::vector<int> new_b;
    new_b.reserve(rb.size() + 1);
    for (int i = 0; i < rb.size(); ++i) {
        if (i == pos_b_) {
            new_b.push_back(u1);
            new_b.push_back(u2);
            new_b.push_back(u3);
        } else if (i == pos_b_ + 1) {
            continue;
        } else {
            new_b.push_back(rb.client(i));
        }
    }

    sol.set_route_clients(route_a_, {});
    sol.set_route_clients(route_b_, {});
    sol.set_route_clients(route_a_, std::move(new_a));
    sol.set_route_clients(route_b_, std::move(new_b));
}

// ===========================================================================
//  Exchange(3,3) -- Swap triple with triple
// ===========================================================================

bool Exchange33::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.size() < 3) {
            continue;
        }

        for (int pa = 0; pa + 2 < route_a.size(); ++pa) {
            int u1 = route_a.client(pa);
            int u2 = route_a.client(pa + 1);
            int u3 = route_a.client(pa + 2);

            auto try_swap = [&](int rb, int pb) {
                // Avoid double-counting.
                if (rb < ra || (rb == ra && pb <= pa + 2)) {
                    return;
                }

                auto const& route_b = sol.route(rb);
                if (pb + 2 >= route_b.size()) {
                    return;
                }

                int v1 = route_b.client(pb);
                int v2 = route_b.client(pb + 1);
                int v3 = route_b.client(pb + 2);

                if (ra == rb) {
                    std::vector<int> new_clients;
                    new_clients.reserve(route_a.size());
                    for (int i = 0; i < route_a.size(); ++i) {
                        if (i == pa) {
                            new_clients.push_back(v1);
                        } else if (i == pa + 1) {
                            new_clients.push_back(v2);
                        } else if (i == pa + 2) {
                            new_clients.push_back(v3);
                        } else if (i == pb) {
                            new_clients.push_back(u1);
                        } else if (i == pb + 1) {
                            new_clients.push_back(u2);
                        } else if (i == pb + 2) {
                            new_clients.push_back(u3);
                        } else {
                            new_clients.push_back(route_a.client(i));
                        }
                    }
                    Route temp(data, route_a.vehicle_type());
                    temp.set_clients(std::move(new_clients));
                    int64_t delta = eval.route_cost(temp) - eval.route_cost(route_a);

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        route_a_ = ra;
                        pos_a_ = pa;
                        route_b_ = rb;
                        pos_b_ = pb;
                    }
                } else {
                    std::vector<int> new_a, new_b;
                    new_a.reserve(route_a.size());
                    new_b.reserve(route_b.size());
                    for (int i = 0; i < route_a.size(); ++i) {
                        if (i == pa) {
                            new_a.push_back(v1);
                        } else if (i == pa + 1) {
                            new_a.push_back(v2);
                        } else if (i == pa + 2) {
                            new_a.push_back(v3);
                        } else {
                            new_a.push_back(route_a.client(i));
                        }
                    }
                    for (int i = 0; i < route_b.size(); ++i) {
                        if (i == pb) {
                            new_b.push_back(u1);
                        } else if (i == pb + 1) {
                            new_b.push_back(u2);
                        } else if (i == pb + 2) {
                            new_b.push_back(u3);
                        } else {
                            new_b.push_back(route_b.client(i));
                        }
                    }

                    Route temp_a(data, route_a.vehicle_type());
                    temp_a.set_clients(std::move(new_a));
                    Route temp_b(data, route_b.vehicle_type());
                    temp_b.set_clients(std::move(new_b));

                    int64_t delta = eval.route_cost(temp_a) + eval.route_cost(temp_b) -
                                    eval.route_cost(route_a) - eval.route_cost(route_b);

                    if (delta < best_delta_) {
                        best_delta_ = delta;
                        route_a_ = ra;
                        pos_a_ = pa;
                        route_b_ = rb;
                        pos_b_ = pb;
                    }
                }
            };

            if (data.granular_k() > 0) {
                std::vector<bool> route_seen(sol.num_routes(), false);
                for (int c : {u1, u2, u3}) {
                    auto nbrs = data.neighbours(c);
                    for (int nb_node : nbrs) {
                        if (nb_node < data.num_depots()) {
                            continue;
                        }
                        int nb_client = nb_node - data.num_depots();
                        auto const& loc = locations[nb_client];
                        if (loc.route < 0 || route_seen[loc.route]) {
                            continue;
                        }
                        route_seen[loc.route] = true;
                        auto const& rb_route = sol.route(loc.route);
                        for (int pb = 0; pb + 2 < rb_route.size(); ++pb) {
                            try_swap(loc.route, pb);
                        }
                    }
                }
            } else {
                for (int rb = ra; rb < sol.num_routes(); ++rb) {
                    auto const& route_b = sol.route(rb);
                    int start = (rb == ra) ? pa + 3 : 0;
                    for (int pb = start; pb + 2 < route_b.size(); ++pb) {
                        try_swap(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void Exchange33::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& ra = sol.route(route_a_);
    auto const& rb = sol.route(route_b_);

    int u1 = ra.client(pos_a_);
    int u2 = ra.client(pos_a_ + 1);
    int u3 = ra.client(pos_a_ + 2);
    int v1 = rb.client(pos_b_);
    int v2 = rb.client(pos_b_ + 1);
    int v3 = rb.client(pos_b_ + 2);

    if (route_a_ == route_b_) {
        std::vector<int> new_clients;
        new_clients.reserve(ra.size());
        for (int i = 0; i < ra.size(); ++i) {
            if (i == pos_a_) {
                new_clients.push_back(v1);
            } else if (i == pos_a_ + 1) {
                new_clients.push_back(v2);
            } else if (i == pos_a_ + 2) {
                new_clients.push_back(v3);
            } else if (i == pos_b_) {
                new_clients.push_back(u1);
            } else if (i == pos_b_ + 1) {
                new_clients.push_back(u2);
            } else if (i == pos_b_ + 2) {
                new_clients.push_back(u3);
            } else {
                new_clients.push_back(ra.client(i));
            }
        }
        sol.set_route_clients(route_a_, std::move(new_clients));
    } else {
        std::vector<int> new_a, new_b;
        new_a.reserve(ra.size());
        new_b.reserve(rb.size());
        for (int i = 0; i < ra.size(); ++i) {
            if (i == pos_a_) {
                new_a.push_back(v1);
            } else if (i == pos_a_ + 1) {
                new_a.push_back(v2);
            } else if (i == pos_a_ + 2) {
                new_a.push_back(v3);
            } else {
                new_a.push_back(ra.client(i));
            }
        }
        for (int i = 0; i < rb.size(); ++i) {
            if (i == pos_b_) {
                new_b.push_back(u1);
            } else if (i == pos_b_ + 1) {
                new_b.push_back(u2);
            } else if (i == pos_b_ + 2) {
                new_b.push_back(u3);
            } else {
                new_b.push_back(rb.client(i));
            }
        }
        sol.set_route_clients(route_a_, {});
        sol.set_route_clients(route_b_, {});
        sol.set_route_clients(route_a_, std::move(new_a));
        sol.set_route_clients(route_b_, std::move(new_b));
    }
}

// ===========================================================================
//  SwapTails
// ===========================================================================

bool SwapTails::find_best_move(Solution const& sol, CostEvaluator const& eval,
                               ProblemData const& data) {
    best_delta_ = 0;
    route_a_ = -1;

    auto locations = build_client_locations(sol);

    for (int ra = 0; ra < sol.num_routes(); ++ra) {
        auto const& route_a = sol.route(ra);
        if (route_a.empty()) {
            continue;
        }

        // Cut after position pa in route A: keep [0..pa], swap [pa+1..end].
        // pa = -1 means swap everything from route A.
        for (int pa = -1; pa < route_a.size(); ++pa) {
            auto try_swap_tail = [&](int rb, int pb) {
                if (rb <= ra) {
                    return;  // avoid double-counting
                }

                auto const& route_b = sol.route(rb);

                // Build new sequences.
                std::vector<int> new_a, new_b;
                for (int i = 0; i <= pa; ++i) {
                    new_a.push_back(route_a.client(i));
                }
                for (int i = pb + 1; i < route_b.size(); ++i) {
                    new_a.push_back(route_b.client(i));
                }

                for (int i = 0; i <= pb; ++i) {
                    new_b.push_back(route_b.client(i));
                }
                for (int i = pa + 1; i < route_a.size(); ++i) {
                    new_b.push_back(route_a.client(i));
                }

                // Evaluate.
                Route temp_a(data, route_a.vehicle_type());
                temp_a.set_clients(std::move(new_a));
                Route temp_b(data, route_b.vehicle_type());
                temp_b.set_clients(std::move(new_b));

                int64_t new_cost = eval.route_cost(temp_a) + eval.route_cost(temp_b);
                int64_t old_cost = eval.route_cost(route_a) + eval.route_cost(route_b);
                int64_t delta = new_cost - old_cost;

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    route_a_ = ra;
                    pos_a_ = pa;
                    route_b_ = rb;
                    pos_b_ = pb;
                }
            };

            if (data.granular_k() > 0) {
                // Find candidate routes by looking at neighbours of
                // the client at/near the cut point.
                int check_client;
                if (pa + 1 < route_a.size()) {
                    check_client = route_a.client(pa + 1);
                } else if (pa >= 0) {
                    check_client = route_a.client(pa);
                } else {
                    continue;
                }

                std::vector<bool> route_seen(sol.num_routes(), false);
                route_seen[ra] = true;

                auto nbrs = data.neighbours(check_client);
                for (int nb_node : nbrs) {
                    if (nb_node < data.num_depots()) {
                        continue;
                    }
                    int nb_client = nb_node - data.num_depots();
                    auto const& loc = locations[nb_client];
                    if (loc.route < 0) {
                        continue;
                    }
                    int rb = loc.route;
                    if (route_seen[rb]) {
                        continue;
                    }
                    route_seen[rb] = true;

                    auto const& route_b = sol.route(rb);
                    if (route_b.empty()) {
                        continue;
                    }
                    for (int pb = -1; pb < route_b.size(); ++pb) {
                        try_swap_tail(rb, pb);
                    }
                }
            } else {
                for (int rb = ra + 1; rb < sol.num_routes(); ++rb) {
                    if (sol.route(rb).empty()) {
                        continue;
                    }
                    for (int pb = -1; pb < sol.route(rb).size(); ++pb) {
                        try_swap_tail(rb, pb);
                    }
                }
            }
        }
    }

    return best_delta_ < 0;
}

void SwapTails::apply(Solution& sol) const {
    assert(route_a_ >= 0);

    auto const& route_a = sol.route(route_a_);
    auto const& route_b = sol.route(route_b_);

    std::vector<int> new_a, new_b;
    for (int i = 0; i <= pos_a_; ++i) {
        new_a.push_back(route_a.client(i));
    }
    for (int i = pos_b_ + 1; i < route_b.size(); ++i) {
        new_a.push_back(route_b.client(i));
    }

    for (int i = 0; i <= pos_b_; ++i) {
        new_b.push_back(route_b.client(i));
    }
    for (int i = pos_a_ + 1; i < route_a.size(); ++i) {
        new_b.push_back(route_a.client(i));
    }

    // Clear both routes first, then assign new sequences to avoid
    // assignment tracking conflicts (see Exchange11::apply comment).
    sol.set_route_clients(route_a_, {});
    sol.set_route_clients(route_b_, {});
    sol.set_route_clients(route_a_, std::move(new_a));
    sol.set_route_clients(route_b_, std::move(new_b));
}

}  // namespace coso
