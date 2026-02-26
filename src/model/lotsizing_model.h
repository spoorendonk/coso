#pragma once

#include "types.h"

namespace coso {

/// Public lot-sizing model API for CLSP / MLCLSP.
class LotSizingModel {
public:
    /// Set number of planning periods.
    void set_num_periods(int periods);

    /// Add a product and return product index.
    int add_product(double setup_cost,
                    double setup_time,
                    double unit_production_cost,
                    double holding_cost);

    /// Set external demand for a product and period.
    void set_demand(int product, int period, double demand);

    /// Set production capacity for a period.
    void set_capacity(int period, double capacity);

    /// Add BOM dependency: producing one unit of `parent` consumes `quantity`
    /// units of `child`.
    void add_bom(int parent, int child, double quantity = 1.0);

    /// Solve with constructive heuristic + local improvements.
    Result solve(TimeLimit tl);

private:
    int num_periods_ = 0;
    int num_products_ = 0;

    struct ProductEntry {
        double setup_cost = 0.0;
        double setup_time = 0.0;
        double unit_production_cost = 0.0;
        double holding_cost = 0.0;
    };

    std::vector<ProductEntry> products_;
    std::vector<std::vector<double>> demands_;
    std::vector<double> capacities_;
    struct BomEntry {
        int parent = -1;
        int child = -1;
        double quantity = 1.0;
    };
    std::vector<BomEntry> bom_;
};

} // namespace coso
