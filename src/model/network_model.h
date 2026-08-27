#pragma once

#include "types.h"

#include <climits>
#include <string>
#include <vector>

namespace coso {

/// Network flow model: declare nodes, arcs, and optional resources, then solve.
///
/// Supports minimum-cost flow style models and resource-constrained variants.
class NetworkModel {
public:
    /// Add a node (positive supply, negative demand).
    int add_node(int supply = 0, std::string name = "");

    /// Add an arc between nodes.
    int add_arc(int tail, int head, int cost = 0, int lower_cap = 0, int upper_cap = INT_MAX);

    /// Add a global resource with an upper bound.
    int add_resource(std::string name = "", int upper_bound = INT_MAX);

    /// Set per-arc resource usage.
    void set_resource_usage(int arc, int resource, int amount);

    /// Solve the network flow problem within the given limits.
    Result solve(TimeLimit tl);

private:
    struct NodeEntry {
        int supply = 0;
        std::string name;
    };
    struct ArcEntry {
        int tail = -1;
        int head = -1;
        int cost = 0;
        int lower_cap = 0;
        int upper_cap = INT_MAX;
    };
    struct ResourceEntry {
        std::string name;
        int upper_bound = INT_MAX;
    };

    std::vector<NodeEntry> nodes_;
    std::vector<ArcEntry> arcs_;
    std::vector<ResourceEntry> resources_;
    std::vector<std::vector<int>> resource_usage_;
};

}  // namespace coso
