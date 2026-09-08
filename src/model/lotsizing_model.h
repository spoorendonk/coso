#pragma once

#include "types.h"

#include <stdexcept>

namespace coso {

/// Public lot-sizing model API for single-level, single-resource CLSP with hard
/// capacity.
class LotSizingModel {
public:
    // -- Stored entry types --------------------------------------------------

    /// A product as declared.
    struct ProductEntry {
        double setup_cost = 0.0;
        double setup_time = 0.0;
        double unit_production_cost = 0.0;
        double holding_cost = 0.0;
    };

    /// Set number of planning periods.
    void set_num_periods(int periods);

    /// Add a product and return product index.
    int add_product(double setup_cost, double setup_time, double unit_production_cost,
                    double holding_cost);

    /// Set external demand for a product and period.
    void set_demand(int product, int period, double demand);

    /// Set production capacity for a period.
    void set_capacity(int period, double capacity);

    /// Solve with constructive heuristic + local improvements.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_periods() const noexcept { return num_periods_; }

    [[nodiscard]] int num_products() const noexcept { return static_cast<int>(products_.size()); }
    [[nodiscard]] ProductEntry const& product(int p) const {
        if (p < 0 || static_cast<size_t>(p) >= products_.size()) {
            throw std::out_of_range("LotSizingModel::product: invalid index");
        }
        return products_[p];
    }

    /// External demand, demands()[product][period].  A row is empty when the
    /// product was added before set_num_periods(), which sizes the rows it
    /// creates; set_num_periods() also wipes every row it finds.
    [[nodiscard]] auto const& demands() const noexcept { return demands_; }

    /// Per-period production capacity; wiped by set_num_periods().
    [[nodiscard]] auto const& capacities() const noexcept { return capacities_; }

private:
    int num_periods_ = 0;
    int num_products_ = 0;

    std::vector<ProductEntry> products_;
    std::vector<std::vector<double>> demands_;
    std::vector<double> capacities_;
};

}  // namespace coso
