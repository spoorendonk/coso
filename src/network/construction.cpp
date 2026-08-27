#include "network/construction.h"

#include <algorithm>
#include <climits>
#include <queue>
#include <vector>

namespace coso {

namespace {

/// BFS/Dijkstra shortest path in the residual graph (non-negative costs only).
/// For negative costs, falls back to Bellman-Ford style.
struct SPResult {
    bool found = false;
    int bottleneck = 0;
    std::vector<int> pred_arc;  // -1 = not reached
};

SPResult shortest_path_bfs(NetworkData const& data, NetworkSolution const& sol, int src, int dst) {
    int const nn = data.num_nodes();
    int const na = data.num_arcs();

    SPResult result;
    result.pred_arc.assign(nn, -1);

    // Use Dijkstra with cost as weights (only forward arcs for greedy).
    // Priority queue: (cost, node).
    std::vector<long long> dist(nn, LLONG_MAX);
    dist[src] = 0;

    using PQEntry = std::pair<long long, int>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<>> pq;
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) {
            continue;
        }
        if (u == dst) {
            break;
        }

        for (int a : data.outgoing(u)) {
            int res = data.arc(a).upper_cap - sol.flow(a);
            if (res <= 0) {
                continue;
            }

            int v = data.arc(a).head;
            long long nd = dist[u] + std::max(0, data.arc(a).cost);
            if (nd < dist[v]) {
                dist[v] = nd;
                result.pred_arc[v] = a;
                pq.push({nd, v});
            }
        }
    }

    if (dist[dst] == LLONG_MAX) {
        result.found = false;
        return result;
    }

    result.found = true;
    result.bottleneck = INT_MAX;

    int cur = dst;
    while (cur != src) {
        int a = result.pred_arc[cur];
        int res = data.arc(a).upper_cap - sol.flow(a);
        result.bottleneck = std::min(result.bottleneck, res);
        cur = data.arc(a).tail;
    }

    return result;
}

}  // anonymous namespace

NetworkSolution construct_greedy(NetworkData const& data) {
    NetworkSolution sol(data);

    // Repeatedly find a supply node and a demand node, route flow between them.
    bool progress = true;
    while (progress) {
        progress = false;

        // Find supply node with largest excess.
        int src = -1;
        int max_excess = 0;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (sol.excess(n) > max_excess) {
                max_excess = sol.excess(n);
                src = n;
            }
        }
        if (src < 0) {
            break;
        }

        // Find demand node with largest deficit.
        int dst = -1;
        int max_deficit = 0;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (-sol.excess(n) > max_deficit) {
                max_deficit = -sol.excess(n);
                dst = n;
            }
        }
        if (dst < 0) {
            break;
        }

        auto path = shortest_path_bfs(data, sol, src, dst);
        if (!path.found || path.bottleneck <= 0) {
            continue;
        }

        int send = std::min({path.bottleneck, sol.excess(src), -sol.excess(dst)});
        if (send <= 0) {
            continue;
        }

        // Augment along the path.
        int cur = dst;
        while (cur != src) {
            int a = path.pred_arc[cur];
            sol.add_flow(a, send);
            cur = data.arc(a).tail;
        }

        progress = true;
    }

    return sol;
}

NetworkSolution construct_feasible(NetworkData const& data) {
    NetworkSolution sol(data);

    // Step 1: Satisfy lower bounds.
    for (int a = 0; a < data.num_arcs(); ++a) {
        if (data.arc(a).lower_cap > 0) {
            sol.set_flow(a, data.arc(a).lower_cap);
        }
    }

    // Step 2: Route remaining excess via greedy augmentation.
    bool progress = true;
    while (progress) {
        progress = false;

        int src = -1;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (sol.excess(n) > 0) {
                src = n;
                break;
            }
        }
        if (src < 0) {
            break;
        }

        int dst = -1;
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (sol.excess(n) < 0) {
                dst = n;
                break;
            }
        }
        if (dst < 0) {
            break;
        }

        auto path = shortest_path_bfs(data, sol, src, dst);
        if (!path.found || path.bottleneck <= 0) {
            continue;
        }

        int send = std::min({path.bottleneck, sol.excess(src), -sol.excess(dst)});
        if (send <= 0) {
            continue;
        }

        int cur = dst;
        while (cur != src) {
            int a = path.pred_arc[cur];
            sol.add_flow(a, send);
            cur = data.arc(a).tail;
        }

        progress = true;
    }

    return sol;
}

}  // namespace coso
