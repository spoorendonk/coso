#pragma once

#include <algorithm>
#include <cassert>
#include <numeric>
#include <vector>

namespace coso {

/// Compiled, immutable representation of a lot sizing instance.
///
/// Carries single-level, single-resource CLSP (Capacitated Lot Sizing Problem)
/// instances.
///
/// Data layout:
///   - Products with demand per period
///   - Production capacity per period
///   - Setup costs and times per product
///   - Holding costs per product per period
class LotsizingData {
public:
    // -------------------------------------------------------------------
    //  Builder
    // -------------------------------------------------------------------

    class Builder {
    public:
        /// Set the number of planning periods.
        Builder& set_num_periods(int T) {
            assert(T > 0);
            num_periods_ = T;
            return *this;
        }

        /// Add a product and return its index.
        /// @param setup_cost  Fixed cost incurred each time a setup is done
        /// @param setup_time  Time consumed by a setup (in capacity units)
        /// @param unit_production_cost  Variable cost per unit produced
        /// @param holding_cost  Holding cost per unit per period
        int add_product(double setup_cost, double setup_time, double unit_production_cost,
                        double holding_cost) {
            int idx = static_cast<int>(setup_costs_.size());
            setup_costs_.push_back(setup_cost);
            setup_times_.push_back(setup_time);
            unit_prod_costs_.push_back(unit_production_cost);
            holding_costs_.push_back(holding_cost);
            return idx;
        }

        /// Set demand for product p in period t.
        Builder& set_demand(int p, int t, double demand) {
            ensure_demand_size_(p, t);
            demands_[p * max_periods_ + t] = demand;
            return *this;
        }

        /// Set production capacity for period t.
        Builder& set_capacity(int t, double capacity) {
            if (t >= static_cast<int>(capacities_.size())) {
                capacities_.resize(t + 1, 0.0);
            }
            capacities_[t] = capacity;
            return *this;
        }

        /// Build the immutable LotsizingData.
        [[nodiscard]] LotsizingData build() const;

    private:
        int num_periods_ = 0;
        int max_periods_ = 0;

        std::vector<double> setup_costs_;
        std::vector<double> setup_times_;
        std::vector<double> unit_prod_costs_;
        std::vector<double> holding_costs_;
        std::vector<double> demands_;  // flat: [p * max_periods_ + t]
        std::vector<double> capacities_;

        void ensure_demand_size_(int p, int t) {
            int needed_periods = t + 1;
            if (needed_periods > max_periods_) {
                // Resize: expand the flat demand array.
                int old_max = max_periods_;
                int num_products = static_cast<int>(setup_costs_.size());
                max_periods_ = needed_periods;
                std::vector<double> new_demands(static_cast<size_t>(num_products) * max_periods_,
                                                0.0);
                for (int pp = 0; pp < num_products; ++pp) {
                    for (int tt = 0; tt < old_max; ++tt) {
                        new_demands[pp * max_periods_ + tt] = demands_[pp * old_max + tt];
                    }
                }
                demands_ = std::move(new_demands);
            }
            int num_products = static_cast<int>(setup_costs_.size());
            size_t needed = static_cast<size_t>(num_products) * max_periods_;
            if (demands_.size() < needed) {
                demands_.resize(needed, 0.0);
            }
        }
    };

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    [[nodiscard]] int num_products() const noexcept { return num_products_; }
    [[nodiscard]] int num_periods() const noexcept { return num_periods_; }

    /// External demand for product p in period t.
    [[nodiscard]] double demand(int p, int t) const {
        assert(p >= 0 && p < num_products_ && t >= 0 && t < num_periods_);
        return demands_[p * num_periods_ + t];
    }

    /// Total demand for product p across all periods.
    [[nodiscard]] double total_demand(int p) const {
        assert(p >= 0 && p < num_products_);
        double sum = 0.0;
        for (int t = 0; t < num_periods_; ++t) {
            sum += demands_[p * num_periods_ + t];
        }
        return sum;
    }

    /// Production capacity in period t.
    [[nodiscard]] double capacity(int t) const {
        assert(t >= 0 && t < num_periods_);
        return capacities_[t];
    }

    /// Setup cost for product p.
    [[nodiscard]] double setup_cost(int p) const {
        assert(p >= 0 && p < num_products_);
        return setup_costs_[p];
    }

    /// Setup time for product p (in capacity units).
    [[nodiscard]] double setup_time(int p) const {
        assert(p >= 0 && p < num_products_);
        return setup_times_[p];
    }

    /// Variable production cost per unit for product p.
    [[nodiscard]] double unit_production_cost(int p) const {
        assert(p >= 0 && p < num_products_);
        return unit_prod_costs_[p];
    }

    /// Holding cost per unit per period for product p.
    [[nodiscard]] double holding_cost(int p) const {
        assert(p >= 0 && p < num_products_);
        return holding_costs_[p];
    }

private:
    int num_products_ = 0;
    int num_periods_ = 0;

    std::vector<double> setup_costs_;
    std::vector<double> setup_times_;
    std::vector<double> unit_prod_costs_;
    std::vector<double> holding_costs_;
    std::vector<double> demands_;  // flat: [p * num_periods_ + t]
    std::vector<double> capacities_;
};

}  // namespace coso
