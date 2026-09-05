#pragma once

#include "types.h"

#include <climits>
#include <string>
#include <vector>

namespace coso {

/// Network flow model: declare nodes with supply and arcs with cost and
/// capacity bounds, then solve.
///
/// Single-commodity minimum-cost flow. Multi-commodity flow and network design
/// are not declarable (see docs/models.md, Network).
class NetworkModel {
public:
    /// Add a node (positive supply, negative demand).
    int add_node(int supply = 0, std::string name = "");

    /// Add an arc between nodes.
    int add_arc(int tail, int head, int cost = 0, int lower_cap = 0, int upper_cap = INT_MAX);

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
    std::vector<NodeEntry> nodes_;
    std::vector<ArcEntry> arcs_;
};

}  // namespace coso
