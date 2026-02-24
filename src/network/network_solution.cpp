#include "network/network_solution.h"

#include <cmath>

namespace coso {

NetworkSolution::NetworkSolution(NetworkData const& data)
    : data_(data)
    , flow_(data.num_arcs(), 0)
    , excess_(data.num_nodes(), 0)
{
    // Initial excess is just the node supply (zero flow everywhere).
    for (int n = 0; n < data.num_nodes(); ++n) {
        excess_[n] = data.supply(n);
    }
}

void NetworkSolution::set_flow(int a, int amount) {
    assert(a >= 0 && a < static_cast<int>(flow_.size()));
    int old = flow_[a];
    int delta = amount - old;
    flow_[a] = amount;

    auto const& ad = data_.arc(a);
    cost_ += static_cast<long long>(delta) * ad.cost;

    excess_[ad.tail] -= delta;  // more outflow from tail
    excess_[ad.head] += delta;  // more inflow to head
}

void NetworkSolution::add_flow(int a, int delta) {
    assert(a >= 0 && a < static_cast<int>(flow_.size()));
    flow_[a] += delta;

    auto const& ad = data_.arc(a);
    cost_ += static_cast<long long>(delta) * ad.cost;

    excess_[ad.tail] -= delta;
    excess_[ad.head] += delta;
}

bool NetworkSolution::feasible() const {
    return flow_conservation() && capacity_feasible() && resource_feasible();
}

bool NetworkSolution::flow_conservation() const {
    for (int n = 0; n < data_.num_nodes(); ++n) {
        if (excess_[n] != 0) return false;
    }
    return true;
}

bool NetworkSolution::capacity_feasible() const {
    for (int a = 0; a < data_.num_arcs(); ++a) {
        auto const& ad = data_.arc(a);
        if (flow_[a] < ad.lower_cap || flow_[a] > ad.upper_cap)
            return false;
    }
    return true;
}

bool NetworkSolution::resource_feasible() const {
    if (!data_.has_resources()) return true;

    for (int r = 0; r < data_.num_resources(); ++r) {
        long long total = 0;
        for (int a = 0; a < data_.num_arcs(); ++a) {
            total += static_cast<long long>(flow_[a]) * data_.resource_usage(a, r);
        }
        if (total > data_.resource(r).upper_bound)
            return false;
    }
    return true;
}

int NetworkSolution::num_excess_violations() const {
    int count = 0;
    for (int n = 0; n < data_.num_nodes(); ++n) {
        if (excess_[n] != 0) ++count;
    }
    return count;
}

int NetworkSolution::num_capacity_violations() const {
    int count = 0;
    for (int a = 0; a < data_.num_arcs(); ++a) {
        auto const& ad = data_.arc(a);
        if (flow_[a] < ad.lower_cap || flow_[a] > ad.upper_cap)
            ++count;
    }
    return count;
}

void NetworkSolution::recompute_excess_() {
    std::fill(excess_.begin(), excess_.end(), 0);
    for (int n = 0; n < data_.num_nodes(); ++n) {
        excess_[n] = data_.supply(n);
    }
    for (int a = 0; a < data_.num_arcs(); ++a) {
        auto const& ad = data_.arc(a);
        excess_[ad.tail] -= flow_[a];
        excess_[ad.head] += flow_[a];
    }
}

} // namespace coso
