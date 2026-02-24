#pragma once

#include <cassert>
#include <climits>
#include <span>
#include <string>
#include <vector>

namespace coso {

/// Compiled, immutable representation of a network flow instance.
///
/// Directed graph with:
///   - Nodes with supply/demand (positive = supply, negative = demand)
///   - Arcs with capacity bounds, unit cost, and optional resource constraints
///   - Resource types (e.g., transit time, fuel) with per-arc consumption
///     and per-path upper bounds
class NetworkData {
public:
    // -------------------------------------------------------------------
    //  Arc data
    // -------------------------------------------------------------------

    struct ArcData {
        int tail         = -1;    ///< source node index
        int head         = -1;    ///< destination node index
        int lower_cap    = 0;     ///< minimum flow on this arc
        int upper_cap    = INT_MAX; ///< maximum flow on this arc
        int cost         = 0;     ///< unit cost per flow on this arc
    };

    // -------------------------------------------------------------------
    //  Resource data
    // -------------------------------------------------------------------

    struct ResourceData {
        std::string name;
        int upper_bound  = INT_MAX; ///< global upper bound on resource consumption
    };

    // -------------------------------------------------------------------
    //  Builder -- the only way to construct a NetworkData
    // -------------------------------------------------------------------

    class Builder {
    public:
        /// Add a node. Returns node index (0-based).
        /// supply > 0 means supply node, supply < 0 means demand node.
        int add_node(int supply = 0, std::string name = "");

        /// Add an arc from tail to head. Returns arc index (0-based).
        int add_arc(int tail, int head, int cost = 0,
                    int lower_cap = 0, int upper_cap = INT_MAX);

        /// Add a resource type. Returns resource index (0-based).
        int add_resource(std::string name = "", int upper_bound = INT_MAX);

        /// Set resource consumption for an arc.
        void set_resource_usage(int arc, int resource, int amount);

        /// Build the immutable NetworkData.
        [[nodiscard]] NetworkData build() const;

    private:
        struct NodeEntry {
            int supply = 0;
            std::string name;
        };
        std::vector<NodeEntry> nodes_;

        struct ArcEntry {
            int tail = -1;
            int head = -1;
            int cost = 0;
            int lower_cap = 0;
            int upper_cap = INT_MAX;
        };
        std::vector<ArcEntry> arcs_;

        struct ResEntry {
            std::string name;
            int upper_bound = INT_MAX;
        };
        std::vector<ResEntry> resources_;

        /// Per-arc resource usage: resource_usage_[arc][resource] = amount.
        std::vector<std::vector<int>> resource_usage_;
    };

    // -------------------------------------------------------------------
    //  Accessors (all const -- NetworkData is immutable after construction)
    // -------------------------------------------------------------------

    [[nodiscard]] int num_nodes()     const noexcept { return num_nodes_; }
    [[nodiscard]] int num_arcs()      const noexcept { return num_arcs_; }
    [[nodiscard]] int num_resources() const noexcept { return num_resources_; }

    /// Supply/demand of node n (positive = supply, negative = demand).
    [[nodiscard]] int supply(int n) const {
        assert(n >= 0 && n < num_nodes_);
        return supply_[n];
    }

    /// Name of node n.
    [[nodiscard]] std::string const& node_name(int n) const {
        assert(n >= 0 && n < num_nodes_);
        return node_names_[n];
    }

    /// Arc data for arc a (0-based).
    [[nodiscard]] ArcData const& arc(int a) const {
        assert(a >= 0 && a < num_arcs_);
        return arcs_[a];
    }

    /// Resource data for resource r (0-based).
    [[nodiscard]] ResourceData const& resource(int r) const {
        assert(r >= 0 && r < num_resources_);
        return resources_[r];
    }

    /// Resource usage for arc a, resource r.
    [[nodiscard]] int resource_usage(int a, int r) const {
        assert(a >= 0 && a < num_arcs_);
        assert(r >= 0 && r < num_resources_);
        return resource_usage_[a * num_resources_ + r];
    }

    /// Outgoing arc indices for node n.
    [[nodiscard]] std::span<int const> outgoing(int n) const {
        assert(n >= 0 && n < num_nodes_);
        return {out_arcs_.data() + out_offset_[n],
                static_cast<size_t>(out_offset_[n + 1] - out_offset_[n])};
    }

    /// Incoming arc indices for node n.
    [[nodiscard]] std::span<int const> incoming(int n) const {
        assert(n >= 0 && n < num_nodes_);
        return {in_arcs_.data() + in_offset_[n],
                static_cast<size_t>(in_offset_[n + 1] - in_offset_[n])};
    }

    /// Whether the instance has any resource constraints.
    [[nodiscard]] bool has_resources() const noexcept {
        return num_resources_ > 0;
    }

    /// Total supply (should equal total demand for a balanced problem).
    [[nodiscard]] int total_supply() const noexcept {
        int s = 0;
        for (int i = 0; i < num_nodes_; ++i) s += supply_[i];
        return s;
    }

private:
    int num_nodes_     = 0;
    int num_arcs_      = 0;
    int num_resources_ = 0;

    std::vector<int>          supply_;
    std::vector<std::string>  node_names_;
    std::vector<ArcData>      arcs_;
    std::vector<ResourceData> resources_;

    /// Flat row-major: resource_usage_[arc * num_resources_ + resource].
    std::vector<int> resource_usage_;

    /// CSR-style adjacency for outgoing arcs.
    std::vector<int> out_offset_;  ///< size num_nodes_+1
    std::vector<int> out_arcs_;

    /// CSR-style adjacency for incoming arcs.
    std::vector<int> in_offset_;   ///< size num_nodes_+1
    std::vector<int> in_arcs_;

    NetworkData() = default;
    friend class Builder;
};

} // namespace coso
