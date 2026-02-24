#pragma once

#include "lotsizing/lotsizing_data.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// A complete solution to a lot sizing problem.
///
/// Tracks production quantities per product per period, setup indicators,
/// and inventory levels. Supports incremental modification and cost queries.
///
/// Layout:
///   production_[p * T + t]  = quantity of product p produced in period t
///   inventory_[p * T + t]   = ending inventory of product p after period t
///   setup_[p * T + t]       = true if product p is set up in period t
class LotsizingSolution {
public:
    /// Construct an empty solution (no production, demands unmet).
    explicit LotsizingSolution(LotsizingData const& data);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The problem data this solution belongs to.
    [[nodiscard]] LotsizingData const& data() const noexcept { return *data_; }

    /// Production quantity for product p in period t.
    [[nodiscard]] double production(int p, int t) const {
        assert(p >= 0 && p < data_->num_products());
        assert(t >= 0 && t < data_->num_periods());
        return production_[p * data_->num_periods() + t];
    }

    /// Ending inventory of product p after period t.
    [[nodiscard]] double inventory(int p, int t) const {
        assert(p >= 0 && p < data_->num_products());
        assert(t >= 0 && t < data_->num_periods());
        return inventory_[p * data_->num_periods() + t];
    }

    /// Whether product p is set up in period t.
    [[nodiscard]] bool setup(int p, int t) const {
        assert(p >= 0 && p < data_->num_products());
        assert(t >= 0 && t < data_->num_periods());
        return setup_[p * data_->num_periods() + t];
    }

    // -------------------------------------------------------------------
    //  Modification
    // -------------------------------------------------------------------

    /// Set production quantity for product p in period t.
    /// Automatically updates setup indicator and recomputes inventory
    /// for product p from period t onward.
    void set_production(int p, int t, double qty);

    /// Add delta to production of product p in period t.
    void add_production(int p, int t, double delta) {
        set_production(p, t, production(p, t) + delta);
    }

    // -------------------------------------------------------------------
    //  Cost computation
    // -------------------------------------------------------------------

    /// Total cost = setup cost + holding cost + production cost.
    [[nodiscard]] double cost() const noexcept { return cost_; }

    /// Total setup cost component.
    [[nodiscard]] double setup_cost() const noexcept { return setup_cost_; }

    /// Total holding cost component.
    [[nodiscard]] double holding_cost() const noexcept { return holding_cost_; }

    /// Total production (variable) cost component.
    [[nodiscard]] double production_cost() const noexcept {
        return production_cost_;
    }

    /// Recompute all costs from scratch (useful after batch modifications).
    void recompute_costs();

    // -------------------------------------------------------------------
    //  Feasibility
    // -------------------------------------------------------------------

    /// Whether the solution is feasible:
    ///   - All demands met (no negative inventory)
    ///   - Capacity not exceeded in any period
    ///   - Production only when setup is active
    [[nodiscard]] bool feasible() const noexcept;

    /// Total capacity usage in period t (production time + setup time).
    [[nodiscard]] double capacity_usage(int t) const;

    /// Remaining capacity in period t.
    [[nodiscard]] double remaining_capacity(int t) const {
        return data_->capacity(t) - capacity_usage(t);
    }

    /// Whether capacity is violated in any period.
    [[nodiscard]] bool has_capacity_violation() const;

    /// Whether any demand is unmet (negative inventory).
    [[nodiscard]] bool has_demand_violation() const;

    /// Total backlog (sum of negative inventories).
    [[nodiscard]] double total_backlog() const;

private:
    LotsizingData const* data_ = nullptr;

    std::vector<double> production_;   // flat: [p * T + t]
    std::vector<double> inventory_;    // flat: [p * T + t]
    std::vector<bool>   setup_;        // flat: [p * T + t]

    double cost_            = 0.0;
    double setup_cost_      = 0.0;
    double holding_cost_    = 0.0;
    double production_cost_ = 0.0;

    /// Recompute inventory for product p from period `from` onward.
    void recompute_inventory_(int p, int from = 0);
};

} // namespace coso
