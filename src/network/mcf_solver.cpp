#include "network/mcf_solver.h"

#include "network/network_operators.h"

#include <algorithm>
#include <climits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace coso {

namespace {

/// True when the uncapacitated arcs (upper_cap INT_MAX) alone contain a
/// negative-cost cycle: flow can then grow around it without bound.
/// Bellman-Ford with every node at distance 0, as from a virtual source.
bool has_uncapacitated_negative_cycle(NetworkData const& data) {
    int const nn = data.num_nodes();
    int const na = data.num_arcs();
    std::vector<long long> dist(nn, 0);
    for (int round = 0; round < nn; ++round) {
        bool relaxed = false;
        for (int a = 0; a < na; ++a) {
            auto const& arc = data.arc(a);
            if (arc.upper_cap != INT_MAX) {
                continue;
            }
            if (dist[arc.tail] + arc.cost < dist[arc.head]) {
                dist[arc.head] = dist[arc.tail] + arc.cost;
                relaxed = true;
            }
        }
        if (!relaxed) {
            return false;
        }
    }
    return true;
}

}  // namespace

McfSolver::PathResult McfSolver::find_augmenting_path(NetworkData const& data,
                                                      NetworkSolution const& sol) {
    int const nn = data.num_nodes();
    int const na = data.num_arcs();

    PathResult result;
    result.dist.assign(nn, LLONG_MAX);
    result.pred_arc.assign(nn, -1);

    // Bellman-Ford with SPFA (Shortest Path Faster Algorithm) queue, seeded
    // with every surplus node at distance 0 -- a super-source joined to each
    // of them by a zero-cost arc.  Searching from one surplus node to one
    // fixed deficit node misses every other pair, and reports a feasible
    // instance infeasible when that one pair is disconnected (#248).
    std::vector<bool> in_queue(nn, false);
    std::queue<int> queue;
    for (int n = 0; n < nn; ++n) {
        if (sol.excess(n) > 0) {
            result.dist[n] = 0;
            queue.push(n);
            in_queue[n] = true;
        }
    }

    // Iteration count for early termination (negative cycle detection).
    std::vector<int> iter_count(nn, 0);

    while (!queue.empty()) {
        int u = queue.front();
        queue.pop();
        in_queue[u] = false;

        // Forward arcs from u.
        for (int a : data.outgoing(u)) {
            int res = data.arc(a).upper_cap - sol.flow(a);
            if (res <= 0) {
                continue;
            }

            int v = data.arc(a).head;
            long long new_dist = result.dist[u] + data.arc(a).cost;
            if (new_dist < result.dist[v]) {
                result.dist[v] = new_dist;
                result.pred_arc[v] = a;  // forward arc
                if (!in_queue[v]) {
                    queue.push(v);
                    in_queue[v] = true;
                    if (++iter_count[v] > nn) {
                        // Negative cycle detected; abort.
                        result.found = false;
                        return result;
                    }
                }
            }
        }

        // Backward arcs from u (reverse of incoming arcs to u).
        // For each arc a: tail -> u, we can push flow back.
        for (int a : data.incoming(u)) {
            int res = sol.flow(a);
            if (res <= 0) {
                continue;
            }

            int v = data.arc(a).tail;
            long long new_dist = result.dist[u] - data.arc(a).cost;
            if (new_dist < result.dist[v]) {
                result.dist[v] = new_dist;
                result.pred_arc[v] = a + na;  // backward arc encoded
                if (!in_queue[v]) {
                    queue.push(v);
                    in_queue[v] = true;
                    if (++iter_count[v] > nn) {
                        result.found = false;
                        return result;
                    }
                }
            }
        }
    }

    // The nearest reachable deficit node; ties go to the lowest index.
    for (int n = 0; n < nn; ++n) {
        if (sol.excess(n) < 0 && result.dist[n] != LLONG_MAX &&
            (result.dst < 0 || result.dist[n] < result.dist[result.dst])) {
            result.dst = n;
        }
    }
    if (result.dst < 0) {
        result.found = false;
        return result;
    }

    // Trace back to the surplus node the path starts from (the root of the
    // shortest-path tree, which has no predecessor) and compute the bottleneck.
    result.found = true;
    result.bottleneck = INT_MAX;

    int cur = result.dst;
    while (result.pred_arc[cur] >= 0) {
        int pa = result.pred_arc[cur];
        if (pa >= na) {
            // Backward arc.
            int a = pa - na;
            int res = sol.flow(a);
            result.bottleneck = std::min(result.bottleneck, res);
            cur = data.arc(a).head;  // predecessor is head of original arc
        } else {
            // Forward arc.
            int res = data.arc(pa).upper_cap - sol.flow(pa);
            result.bottleneck = std::min(result.bottleneck, res);
            cur = data.arc(pa).tail;
        }
    }
    result.src = cur;

    return result;
}

NetworkSolution McfSolver::solve(NetworkData const& data) {
    NetworkSolution sol(data);
    int const na = data.num_arcs();

    // SSP needs a residual graph with no negative-cost cycle, and augmenting
    // along shortest paths keeps it so.  Cancel every negative cycle of the
    // zero flow first; this leaves each node's excess unchanged (#253).  A
    // cycle with no finite capacity on it can carry unbounded flow, so the
    // cost has no minimum.  That is checked on the arcs themselves, before
    // any cancellation: once flow is on an uncapacitated arc its residual is
    // finite, and the cycle would no longer look unbounded.  With no such
    // cycle the cost is bounded below, and each cancellation lowers it by at
    // least 1, so the loop needs no iteration cap.
    if (has_uncapacitated_negative_cycle(data)) {
        throw std::invalid_argument(
            "McfSolver::solve: a negative-cost cycle has no finite capacity, so the cost "
            "is unbounded below");
    }
    while (true) {
        auto move = CycleCancel::find_negative_cycle(data, sol);
        if (move.amount <= 0) {
            break;
        }
        CycleCancel::apply(data, sol, move);
    }

    // Successive shortest paths: repeatedly augment along a cheapest path from
    // any surplus node to any deficit node.  Stops when supply is all routed,
    // or when no surplus node can reach any deficit node in the residual graph,
    // which is when the instance is infeasible.
    while (true) {
        auto path = find_augmenting_path(data, sol);
        if (!path.found || path.bottleneck <= 0) {
            break;
        }
        int const src = path.src;
        int const dst = path.dst;

        // Limit flow to the min of supply at src and demand at dst.
        int send = std::min({path.bottleneck, sol.excess(src), -sol.excess(dst)});
        if (send <= 0) {
            break;
        }

        // Augment along the path.
        int cur = dst;
        while (cur != src) {
            int pa = path.pred_arc[cur];
            if (pa >= na) {
                // Backward arc: reduce flow.
                int a = pa - na;
                sol.add_flow(a, -send);
                cur = data.arc(a).head;
            } else {
                // Forward arc: increase flow.
                sol.add_flow(pa, send);
                cur = data.arc(pa).tail;
            }
        }
    }

    return sol;
}

}  // namespace coso
