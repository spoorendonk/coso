#include "scheduling/disjunctive_graph.h"

#include <algorithm>
#include <cassert>
#include <queue>

namespace coso {

DisjunctiveGraph::DisjunctiveGraph(int num_jobs, int num_machines)
    : num_jobs_(num_jobs),
      num_machines_(num_machines),
      job_ops_(num_jobs),
      machine_seq_(num_machines) {}

int DisjunctiveGraph::add_operation(int job, int machine, int duration) {
    assert(job >= 0 && job < num_jobs_);
    assert(machine >= -1 && machine < num_machines_);
    assert(duration >= 0);

    int id = static_cast<int>(ops_.size());
    ops_.push_back({job, machine, duration});
    job_ops_[job].push_back(id);
    dirty_ = true;
    return id;
}

void DisjunctiveGraph::set_processing_time(int op, int machine, int duration) {
    assert(op >= 0 && op < num_operations());
    assert(machine >= -1 && machine < num_machines_);
    assert(duration >= 0);

    ops_[op].machine = machine;
    ops_[op].duration = duration;
    dirty_ = true;
}

void DisjunctiveGraph::set_sequence(int machine,
                                     std::vector<int> const& ops) {
    assert(machine >= 0 && machine < num_machines_);
    machine_seq_[machine] = ops;
    dirty_ = true;
}

// -------------------------------------------------------------------------- //
//  Internal graph rebuild + forward/backward pass                             //
// -------------------------------------------------------------------------- //

std::vector<int> DisjunctiveGraph::topo_sort() const {
    int n = num_operations() + 2;  // ops + source + sink

    // Build in-degree from adj_.
    std::vector<int> in_deg(n, 0);
    for (int u = 0; u < n; ++u)
        for (int v : adj_[u])
            ++in_deg[v];

    std::queue<int> q;
    for (int u = 0; u < n; ++u)
        if (in_deg[u] == 0)
            q.push(u);

    std::vector<int> order;
    order.reserve(n);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        order.push_back(u);
        for (int v : adj_[u])
            if (--in_deg[v] == 0)
                q.push(v);
    }

    assert(static_cast<int>(order.size()) == n);
    return order;
}

void DisjunctiveGraph::recompute() {
    if (!dirty_)
        return;

    int n = num_operations() + 2;
    int src = source();
    int snk = sink();

    // Rebuild adjacency list from scratch.
    adj_.assign(n, {});

    // Conjunctive arcs: job precedences.
    for (int j = 0; j < num_jobs_; ++j) {
        auto const& ops = job_ops_[j];
        if (ops.empty())
            continue;
        // source -> first op of job
        adj_[src].push_back(ops.front());
        // last op of job -> sink
        adj_[ops.back()].push_back(snk);
        // chain within job
        for (int i = 0; i + 1 < static_cast<int>(ops.size()); ++i)
            adj_[ops[i]].push_back(ops[i + 1]);
    }

    // Disjunctive arcs: machine sequences.
    for (int m = 0; m < num_machines_; ++m) {
        auto const& seq = machine_seq_[m];
        for (int i = 0; i + 1 < static_cast<int>(seq.size()); ++i)
            adj_[seq[i]].push_back(seq[i + 1]);
    }

    // Forward and backward pass.
    auto topo = topo_sort();
    forward_pass(topo);
    backward_pass(topo);

    dirty_ = false;
}

void DisjunctiveGraph::forward_pass(std::vector<int> const& topo) {
    int n = num_operations() + 2;
    earliest_start_.assign(n, 0);
    earliest_finish_.assign(n, 0);

    // Source and sink have duration 0.
    for (int u : topo) {
        int dur = (u < num_operations()) ? ops_[u].duration : 0;
        earliest_finish_[u] = earliest_start_[u] + dur;
        for (int v : adj_[u])
            earliest_start_[v] =
                std::max(earliest_start_[v], earliest_finish_[u]);
    }
}

void DisjunctiveGraph::backward_pass(std::vector<int> const& topo) {
    int n = num_operations() + 2;
    int makespan = earliest_start_[sink()];

    // latest_finish[u] = min over successors v of latest_start[v]
    // latest_start[u]  = latest_finish[u] - dur(u)
    std::vector<int> latest_finish(n, makespan);
    latest_start_.assign(n, makespan);

    for (int i = static_cast<int>(topo.size()) - 1; i >= 0; --i) {
        int u = topo[i];
        int dur = (u < num_operations()) ? ops_[u].duration : 0;
        for (int v : adj_[u])
            latest_finish[u] = std::min(latest_finish[u], latest_start_[v]);
        latest_start_[u] = latest_finish[u] - dur;
    }
}

// -------------------------------------------------------------------------- //
//  Public queries                                                             //
// -------------------------------------------------------------------------- //

int DisjunctiveGraph::start_time(int op) {
    assert(op >= 0 && op < num_operations());
    recompute();
    return earliest_start_[op];
}

int DisjunctiveGraph::completion_time(int op) {
    assert(op >= 0 && op < num_operations());
    recompute();
    return earliest_finish_[op];
}

int DisjunctiveGraph::latest_start_time(int op) {
    assert(op >= 0 && op < num_operations());
    recompute();
    return latest_start_[op];
}

int DisjunctiveGraph::critical_path() {
    recompute();
    return earliest_start_[sink()];
}

std::vector<int> DisjunctiveGraph::critical_path_ops() {
    recompute();

    std::vector<int> path;
    for (int op = 0; op < num_operations(); ++op) {
        if (earliest_start_[op] == latest_start_[op])
            path.push_back(op);
    }
    return path;
}

std::vector<int> DisjunctiveGraph::topological_order() {
    recompute();

    auto full = topo_sort();
    std::vector<int> result;
    result.reserve(num_operations());
    int src = source();
    int snk = sink();
    for (int u : full)
        if (u != src && u != snk)
            result.push_back(u);
    return result;
}

int DisjunctiveGraph::longest_path(int from, int to) {
    recompute();

    // BFS/DP on the DAG in topological order.
    auto topo = topo_sort();
    int n = num_operations() + 2;
    std::vector<int> dist(n, -1);
    dist[from] = 0;

    for (int u : topo) {
        if (dist[u] < 0)
            continue;
        int dur = (u < num_operations()) ? ops_[u].duration : 0;
        for (int v : adj_[u])
            dist[v] = std::max(dist[v], dist[u] + dur);
    }

    return dist[to];
}

} // namespace coso
