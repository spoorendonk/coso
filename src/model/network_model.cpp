#include "model/network_model.h"

#include "common/work_units.h"
#include "network/mcf_solver.h"
#include "network/network_data.h"
#include "search/stop_criterion.h"

#include <chrono>
#include <stdexcept>
#include <vector>

namespace coso {

int NetworkModel::add_node(int supply, std::string name) {
    int idx = static_cast<int>(nodes_.size());
    nodes_.push_back({supply, std::move(name)});
    return idx;
}

int NetworkModel::add_arc(int tail, int head, int cost, int upper_cap) {
    int n = static_cast<int>(nodes_.size());
    if (tail < 0 || tail >= n || head < 0 || head >= n) {
        throw std::out_of_range("NetworkModel::add_arc: invalid node index");
    }
    if (upper_cap < 0) {
        throw std::invalid_argument("NetworkModel::add_arc: invalid capacity bounds");
    }

    int idx = static_cast<int>(arcs_.size());
    arcs_.push_back({tail, head, cost, upper_cap});
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
        builder.add_arc(a.tail, a.head, a.cost, a.upper_cap);
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
    // NetworkData keeps the arcs in declaration order, so arc a here is arc a
    // as the user declared it.  The model has one commodity (#184).
    auto& flows = result.arc_flows_.emplace_back(data.num_arcs(), 0.0);
    for (int a = 0; a < data.num_arcs(); ++a) {
        flows[a] = static_cast<double>(sol.flow(a));
    }
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

}  // namespace coso
