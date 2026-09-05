#include "model/network_model.h"

#include "common/work_units.h"
#include "network/mcf_solver.h"
#include "network/network_data.h"
#include "search/stop_criterion.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <vector>

namespace coso {

namespace {

std::vector<Result::PathFlow> decompose_paths(NetworkData const& data, NetworkSolution const& sol) {
    std::vector<Result::PathFlow> paths;
    std::vector<int> remaining(data.num_arcs(), 0);
    for (int a = 0; a < data.num_arcs(); ++a) {
        remaining[a] = std::max(0, sol.flow(a));
    }

    auto has_outgoing_flow = [&](int n) -> bool {
        for (int a : data.outgoing(n)) {
            if (remaining[a] > 0) {
                return true;
            }
        }
        return false;
    };

    auto pick_start = [&]() -> int {
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (data.supply(n) > 0 && has_outgoing_flow(n)) {
                return n;
            }
        }
        for (int n = 0; n < data.num_nodes(); ++n) {
            if (has_outgoing_flow(n)) {
                return n;
            }
        }
        return -1;
    };

    while (true) {
        int start = pick_start();
        if (start < 0) {
            break;
        }

        std::vector<int> path{start};
        std::vector<int> used_arcs;
        std::vector<bool> seen(data.num_nodes(), false);
        seen[start] = true;

        int cur = start;
        int bottleneck = std::numeric_limits<int>::max();

        while (true) {
            int chosen = -1;
            for (int a : data.outgoing(cur)) {
                if (remaining[a] <= 0) {
                    continue;
                }
                chosen = a;
                if (data.supply(data.arc(a).head) < 0) {
                    break;
                }
            }
            if (chosen < 0) {
                break;
            }

            used_arcs.push_back(chosen);
            bottleneck = std::min(bottleneck, remaining[chosen]);

            int next = data.arc(chosen).head;
            path.push_back(next);

            if (data.supply(next) < 0) {
                break;
            }
            if (seen[next]) {
                break;
            }
            seen[next] = true;
            cur = next;
        }

        if (used_arcs.empty() || bottleneck <= 0 || bottleneck == std::numeric_limits<int>::max()) {
            break;
        }

        for (int a : used_arcs) {
            remaining[a] -= bottleneck;
        }

        paths.push_back(Result::PathFlow{
            .path = std::move(path),
            .flow = static_cast<double>(bottleneck),
        });
    }

    return paths;
}

}  // namespace

int NetworkModel::add_node(int supply, std::string name) {
    int idx = static_cast<int>(nodes_.size());
    nodes_.push_back({supply, std::move(name)});
    return idx;
}

int NetworkModel::add_arc(int tail, int head, int cost, int lower_cap, int upper_cap) {
    int n = static_cast<int>(nodes_.size());
    if (tail < 0 || tail >= n || head < 0 || head >= n) {
        throw std::out_of_range("NetworkModel::add_arc: invalid node index");
    }
    if (lower_cap < 0 || upper_cap < lower_cap) {
        throw std::invalid_argument("NetworkModel::add_arc: invalid capacity bounds");
    }

    int idx = static_cast<int>(arcs_.size());
    arcs_.push_back({tail, head, cost, lower_cap, upper_cap});
    return idx;
}

Result NetworkModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    if (nodes_.empty() || arcs_.empty()) {
        return {};
    }

    NetworkData::Builder builder;

    for (auto const& n : nodes_) {
        builder.add_node(n.supply, n.name);
        work.count(1);
    }
    for (auto const& a : arcs_) {
        builder.add_arc(a.tail, a.head, a.cost, a.lower_cap, a.upper_cap);
        work.count(1);
    }

    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    NetworkData data = builder.build();
    work.count(static_cast<uint64_t>(data.num_nodes()) + static_cast<uint64_t>(data.num_arcs()));

    NetworkSolution sol = McfSolver::solve(data);
    work.count(static_cast<uint64_t>(data.num_arcs()));

    Result result;
    result.feasible_ = sol.feasible();
    result.cost_ = static_cast<double>(sol.cost());
    result.flows_.push_back(decompose_paths(data, sol));
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

}  // namespace coso
