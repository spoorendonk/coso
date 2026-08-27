#include "network/mcf_solver.h"

#include <algorithm>
#include <climits>
#include <queue>
#include <vector>

namespace coso {

McfSolver::PathResult McfSolver::find_shortest_path(NetworkData const& data,
                                                    NetworkSolution const& sol, int src, int dst) {
    int const nn = data.num_nodes();
    int const na = data.num_arcs();

    PathResult result;
    result.dist.assign(nn, LLONG_MAX);
    result.pred_arc.assign(nn, -1);
    result.dist[src] = 0;

    // Bellman-Ford with SPFA (Shortest Path Faster Algorithm) queue.
    std::vector<bool> in_queue(nn, false);
    std::queue<int> queue;
    queue.push(src);
    in_queue[src] = true;

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
            int res = sol.flow(a) - data.arc(a).lower_cap;
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

    if (result.dist[dst] == LLONG_MAX) {
        result.found = false;
        return result;
    }

    // Trace back path and compute bottleneck.
    result.found = true;
    result.bottleneck = INT_MAX;

    int cur = dst;
    while (cur != src) {
        int pa = result.pred_arc[cur];
        if (pa >= na) {
            // Backward arc.
            int a = pa - na;
            int res = sol.flow(a) - data.arc(a).lower_cap;
            result.bottleneck = std::min(result.bottleneck, res);
            cur = data.arc(a).head;  // predecessor is head of original arc
        } else {
            // Forward arc.
            int res = data.arc(pa).upper_cap - sol.flow(pa);
            result.bottleneck = std::min(result.bottleneck, res);
            cur = data.arc(pa).tail;
        }
    }

    return result;
}

NetworkSolution McfSolver::solve(NetworkData const& data) {
    NetworkSolution sol(data);
    int const na = data.num_arcs();

    // Handle lower bounds: push mandatory flow on arcs with lower_cap > 0.
    for (int a = 0; a < na; ++a) {
        if (data.arc(a).lower_cap > 0) {
            sol.set_flow(a, data.arc(a).lower_cap);
        }
    }

    // Collect supply and demand nodes.
    // After lower-bound handling, excess might have changed.
    // We need to route flow from nodes with positive excess to negative excess.

    // Successive shortest paths: repeatedly find cheapest augmenting paths.
    bool progress = true;
    while (progress) {
        progress = false;

        // Find a node with positive excess (supply not yet routed).
        int src = -1;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (sol.excess(n) > 0) {
                src = n;
                break;
            }
        }
        if (src < 0) {
            break;  // no more supply to route
        }

        // Find a node with negative excess (unmet demand).
        int dst = -1;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (sol.excess(n) < 0) {
                dst = n;
                break;
            }
        }
        if (dst < 0) {
            break;  // no more demand
        }

        // Find shortest path in residual graph.
        auto path = find_shortest_path(data, sol, src, dst);
        if (!path.found || path.bottleneck <= 0) {
            continue;
        }

        // Limit flow to the min of supply at src and demand at dst.
        int send = std::min({path.bottleneck, sol.excess(src), -sol.excess(dst)});
        if (send <= 0) {
            continue;
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

        progress = true;
    }

    return sol;
}

}  // namespace coso
