#include "network/network_operators.h"

#include <algorithm>
#include <climits>
#include <queue>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  RerouteFlow
// ---------------------------------------------------------------------------

std::vector<RerouteFlowMove>
RerouteFlow::enumerate(NetworkData const& data, NetworkSolution const& sol) {
    std::vector<RerouteFlowMove> moves;
    int const na = data.num_arcs();
    int const nn = data.num_nodes();

    for (int a = 0; a < na; ++a) {
        if (sol.flow(a) <= 0) continue;

        int tail = data.arc(a).tail;
        int head = data.arc(a).head;
        int arc_cost = data.arc(a).cost;

        // Find shortest path from tail to head in the residual graph
        // excluding arc a itself.
        std::vector<long long> dist(nn, LLONG_MAX);
        std::vector<int> pred(nn, -1);
        dist[tail] = 0;

        using PQE = std::pair<long long, int>;
        std::priority_queue<PQE, std::vector<PQE>, std::greater<>> pq;
        pq.push({0, tail});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;
            if (u == head) break;

            for (int a2 : data.outgoing(u)) {
                if (a2 == a) continue;  // skip the arc being rerouted
                int res = data.arc(a2).upper_cap - sol.flow(a2);
                if (res <= 0) continue;
                int v = data.arc(a2).head;
                long long nd = dist[u] + data.arc(a2).cost;
                if (nd < dist[v]) {
                    dist[v] = nd;
                    pred[v] = a2;
                    pq.push({nd, v});
                }
            }
        }

        if (dist[head] == LLONG_MAX) continue;

        // Check if alternative path is cheaper.
        if (dist[head] >= arc_cost) continue;

        // Build the move.
        RerouteFlowMove move;
        move.source_node = tail;
        move.sink_node = head;
        move.amount = 1;  // reroute 1 unit at a time
        move.old_arcs.push_back(a);
        move.delta = dist[head] - arc_cost;

        // Compute bottleneck along the new path.
        int bottleneck = sol.flow(a);
        int cur = head;
        while (cur != tail) {
            int pa = pred[cur];
            int res = data.arc(pa).upper_cap - sol.flow(pa);
            bottleneck = std::min(bottleneck, res);
            move.new_arcs.push_back(pa);
            cur = data.arc(pa).tail;
        }

        move.amount = bottleneck;
        move.delta = (dist[head] - arc_cost) * bottleneck;

        if (move.amount > 0 && move.delta < 0) {
            // Reverse new_arcs so they go from source to sink.
            std::reverse(move.new_arcs.begin(), move.new_arcs.end());
            moves.push_back(std::move(move));
        }
    }

    return moves;
}

void RerouteFlow::apply(NetworkSolution& sol, RerouteFlowMove const& move) {
    for (int a : move.old_arcs) {
        sol.add_flow(a, -move.amount);
    }
    for (int a : move.new_arcs) {
        sol.add_flow(a, move.amount);
    }
}

long long RerouteFlow::evaluate(
    NetworkData const& data,
    NetworkSolution const& sol,
    int arc)
{
    if (sol.flow(arc) <= 0) return 0;

    int tail = data.arc(arc).tail;
    int head = data.arc(arc).head;
    int nn = data.num_nodes();

    // Dijkstra from tail to head excluding arc.
    std::vector<long long> dist(nn, LLONG_MAX);
    dist[tail] = 0;

    using PQE = std::pair<long long, int>;
    std::priority_queue<PQE, std::vector<PQE>, std::greater<>> pq;
    pq.push({0, tail});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;
        if (u == head) break;

        for (int a : data.outgoing(u)) {
            if (a == arc) continue;
            int res = data.arc(a).upper_cap - sol.flow(a);
            if (res <= 0) continue;
            int v = data.arc(a).head;
            long long nd = dist[u] + data.arc(a).cost;
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }

    if (dist[head] == LLONG_MAX) return 0;
    return dist[head] - data.arc(arc).cost;
}

// ---------------------------------------------------------------------------
//  AdjustCapacity
// ---------------------------------------------------------------------------

std::vector<AdjustCapacityMove>
AdjustCapacity::enumerate(NetworkData const& data, NetworkSolution const& sol) {
    std::vector<AdjustCapacityMove> moves;

    for (int a = 0; a < data.num_arcs(); ++a) {
        auto const& ad = data.arc(a);
        int f = sol.flow(a);

        // Try reducing flow to lower bound if cost is positive.
        if (ad.cost > 0 && f > ad.lower_cap) {
            AdjustCapacityMove m;
            m.arc = a;
            m.new_flow = ad.lower_cap;
            m.delta = static_cast<long long>(ad.lower_cap - f) * ad.cost;
            moves.push_back(m);
        }

        // Try increasing flow to upper bound if cost is negative.
        if (ad.cost < 0 && f < ad.upper_cap) {
            AdjustCapacityMove m;
            m.arc = a;
            m.new_flow = ad.upper_cap;
            m.delta = static_cast<long long>(ad.upper_cap - f) * ad.cost;
            moves.push_back(m);
        }
    }

    return moves;
}

void AdjustCapacity::apply(NetworkSolution& sol,
                           AdjustCapacityMove const& move) {
    sol.set_flow(move.arc, move.new_flow);
}

// ---------------------------------------------------------------------------
//  CycleCancel
// ---------------------------------------------------------------------------

CycleCancelMove
CycleCancel::find_negative_cycle(NetworkData const& data,
                                 NetworkSolution const& sol) {
    int const nn = data.num_nodes();
    int const na = data.num_arcs();

    // Bellman-Ford on the residual graph to detect negative cycles.
    // Residual arcs:
    //   Forward: arc a, cost = cost[a], residual = upper_cap - flow
    //   Backward: for arc a with flow > lower_cap, cost = -cost[a]

    std::vector<long long> dist(nn, 0);  // start at 0 to find any neg cycle
    std::vector<int> pred_arc(nn, -1);
    std::vector<int> pred_node(nn, -1);

    int last_relaxed = -1;

    for (int iter = 0; iter < nn; ++iter) {
        last_relaxed = -1;

        // Forward arcs.
        for (int a = 0; a < na; ++a) {
            int res = data.arc(a).upper_cap - sol.flow(a);
            if (res <= 0) continue;
            int u = data.arc(a).tail;
            int v = data.arc(a).head;
            long long nd = dist[u] + data.arc(a).cost;
            if (nd < dist[v]) {
                dist[v] = nd;
                pred_arc[v] = a;
                pred_node[v] = u;
                last_relaxed = v;
            }
        }

        // Backward arcs.
        for (int a = 0; a < na; ++a) {
            int res = sol.flow(a) - data.arc(a).lower_cap;
            if (res <= 0) continue;
            int u = data.arc(a).head;  // backward: from head to tail
            int v = data.arc(a).tail;
            long long nd = dist[u] - data.arc(a).cost;
            if (nd < dist[v]) {
                dist[v] = nd;
                pred_arc[v] = a + na;  // encode backward
                pred_node[v] = u;
                last_relaxed = v;
            }
        }
    }

    CycleCancelMove move;
    if (last_relaxed < 0) return move;  // no negative cycle

    // Trace back the cycle.
    std::vector<bool> visited(nn, false);
    int cur = last_relaxed;

    // Walk back nn steps to ensure we're on the cycle.
    for (int i = 0; i < nn; ++i) {
        cur = pred_node[cur];
    }

    // Now trace the cycle.
    int cycle_start = cur;
    move.amount = INT_MAX;
    move.delta = 0;

    do {
        int pa = pred_arc[cur];
        move.cycle_arcs.push_back(pa);

        if (pa >= na) {
            int a = pa - na;
            int res = sol.flow(a) - data.arc(a).lower_cap;
            move.amount = std::min(move.amount, res);
            move.delta -= data.arc(a).cost;
            cur = pred_node[cur];
        } else {
            int res = data.arc(pa).upper_cap - sol.flow(pa);
            move.amount = std::min(move.amount, res);
            move.delta += data.arc(pa).cost;
            cur = pred_node[cur];
        }
    } while (cur != cycle_start);

    move.delta *= move.amount;

    if (move.amount <= 0 || move.delta >= 0) {
        // Not actually improving.
        move.amount = 0;
        move.cycle_arcs.clear();
        move.delta = 0;
    }

    return move;
}

void CycleCancel::apply(NetworkData const& data, NetworkSolution& sol,
                        CycleCancelMove const& move) {
    int const na = data.num_arcs();
    for (int pa : move.cycle_arcs) {
        if (pa >= na) {
            sol.add_flow(pa - na, -move.amount);
        } else {
            sol.add_flow(pa, move.amount);
        }
    }
}

int CycleCancel::cancel_all(NetworkData const& data, NetworkSolution& sol) {
    int count = 0;
    int const max_iter = data.num_nodes() * data.num_arcs();  // safety bound

    for (int iter = 0; iter < max_iter; ++iter) {
        auto move = find_negative_cycle(data, sol);
        if (move.amount <= 0) break;
        apply(data, sol, move);
        ++count;
    }

    return count;
}

} // namespace coso
