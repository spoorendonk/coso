#include "network/network_data.h"

#include <algorithm>
#include <numeric>

namespace coso {

// ---------------------------------------------------------------------------
//  Builder
// ---------------------------------------------------------------------------

int NetworkData::Builder::add_node(int supply, std::string name) {
    int idx = static_cast<int>(nodes_.size());
    nodes_.push_back({supply, std::move(name)});
    return idx;
}

int NetworkData::Builder::add_arc(int tail, int head, int cost,
                                  int lower_cap, int upper_cap) {
    int idx = static_cast<int>(arcs_.size());
    arcs_.push_back({tail, head, cost, lower_cap, upper_cap});
    return idx;
}

int NetworkData::Builder::add_resource(std::string name, int upper_bound) {
    int idx = static_cast<int>(resources_.size());
    resources_.push_back({std::move(name), upper_bound});
    return idx;
}

void NetworkData::Builder::set_resource_usage(int arc, int resource,
                                              int amount) {
    assert(arc >= 0 && arc < static_cast<int>(arcs_.size()));
    assert(resource >= 0 && resource < static_cast<int>(resources_.size()));

    // Grow the per-arc vector as needed.
    if (resource_usage_.size() <= static_cast<size_t>(arc))
        resource_usage_.resize(arc + 1);
    if (resource_usage_[arc].size() <= static_cast<size_t>(resource))
        resource_usage_[arc].resize(resource + 1, 0);

    resource_usage_[arc][resource] = amount;
}

NetworkData NetworkData::Builder::build() const {
    NetworkData data;

    int const nn = static_cast<int>(nodes_.size());
    int const na = static_cast<int>(arcs_.size());
    int const nr = static_cast<int>(resources_.size());

    data.num_nodes_     = nn;
    data.num_arcs_      = na;
    data.num_resources_ = nr;

    // Copy node data.
    data.supply_.resize(nn);
    data.node_names_.resize(nn);
    for (int i = 0; i < nn; ++i) {
        data.supply_[i]     = nodes_[i].supply;
        data.node_names_[i] = nodes_[i].name;
    }

    // Copy arc data.
    data.arcs_.resize(na);
    for (int a = 0; a < na; ++a) {
        data.arcs_[a].tail      = arcs_[a].tail;
        data.arcs_[a].head      = arcs_[a].head;
        data.arcs_[a].cost      = arcs_[a].cost;
        data.arcs_[a].lower_cap = arcs_[a].lower_cap;
        data.arcs_[a].upper_cap = arcs_[a].upper_cap;
    }

    // Copy resource data.
    data.resources_.resize(nr);
    for (int r = 0; r < nr; ++r) {
        data.resources_[r].name        = resources_[r].name;
        data.resources_[r].upper_bound = resources_[r].upper_bound;
    }

    // Copy resource usage into flat array.
    data.resource_usage_.assign(na * nr, 0);
    for (int a = 0; a < static_cast<int>(resource_usage_.size()); ++a) {
        for (int r = 0; r < static_cast<int>(resource_usage_[a].size()); ++r) {
            data.resource_usage_[a * nr + r] = resource_usage_[a][r];
        }
    }

    // Build CSR outgoing adjacency.
    std::vector<int> out_count(nn, 0);
    for (int a = 0; a < na; ++a)
        ++out_count[arcs_[a].tail];

    data.out_offset_.resize(nn + 1);
    data.out_offset_[0] = 0;
    for (int i = 0; i < nn; ++i)
        data.out_offset_[i + 1] = data.out_offset_[i] + out_count[i];

    data.out_arcs_.resize(na);
    std::fill(out_count.begin(), out_count.end(), 0);
    for (int a = 0; a < na; ++a) {
        int t = arcs_[a].tail;
        int pos = data.out_offset_[t] + out_count[t];
        data.out_arcs_[pos] = a;
        ++out_count[t];
    }

    // Build CSR incoming adjacency.
    std::vector<int> in_count(nn, 0);
    for (int a = 0; a < na; ++a)
        ++in_count[arcs_[a].head];

    data.in_offset_.resize(nn + 1);
    data.in_offset_[0] = 0;
    for (int i = 0; i < nn; ++i)
        data.in_offset_[i + 1] = data.in_offset_[i] + in_count[i];

    data.in_arcs_.resize(na);
    std::fill(in_count.begin(), in_count.end(), 0);
    for (int a = 0; a < na; ++a) {
        int h = arcs_[a].head;
        int pos = data.in_offset_[h] + in_count[h];
        data.in_arcs_[pos] = a;
        ++in_count[h];
    }

    return data;
}

} // namespace coso
