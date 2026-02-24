#include "lotsizing/lotsizing_solution.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace coso {

LotsizingSolution::LotsizingSolution(LotsizingData const& data)
    : data_(&data)
{
    int P = data.num_products();
    int T = data.num_periods();
    size_t n = static_cast<size_t>(P) * T;

    production_.assign(n, 0.0);
    inventory_.assign(n, 0.0);
    setup_.assign(n, false);

    // With zero production, inventory tracks negative demand (backlog).
    for (int p = 0; p < P; ++p) {
        recompute_inventory_(p);
    }

    recompute_costs();
}

// ---------------------------------------------------------------------------
//  Modification
// ---------------------------------------------------------------------------

void LotsizingSolution::set_production(int p, int t, double qty)
{
    assert(p >= 0 && p < data_->num_products());
    assert(t >= 0 && t < data_->num_periods());

    int T = data_->num_periods();
    production_[p * T + t] = std::max(0.0, qty);
    setup_[p * T + t] = (qty > 0.0);

    recompute_inventory_(p, t);
    recompute_costs();
}

// ---------------------------------------------------------------------------
//  Cost computation
// ---------------------------------------------------------------------------

void LotsizingSolution::recompute_costs()
{
    int P = data_->num_products();
    int T = data_->num_periods();

    setup_cost_ = 0.0;
    holding_cost_ = 0.0;
    production_cost_ = 0.0;

    for (int p = 0; p < P; ++p) {
        for (int t = 0; t < T; ++t) {
            int idx = p * T + t;
            if (setup_[idx])
                setup_cost_ += data_->setup_cost(p);
            if (inventory_[idx] > 0.0)
                holding_cost_ += data_->holding_cost(p) * inventory_[idx];
            production_cost_ += data_->unit_production_cost(p) * production_[idx];
        }
    }

    cost_ = setup_cost_ + holding_cost_ + production_cost_;
}

// ---------------------------------------------------------------------------
//  Feasibility
// ---------------------------------------------------------------------------

bool LotsizingSolution::feasible() const noexcept
{
    return !has_demand_violation() && !has_capacity_violation();
}

double LotsizingSolution::capacity_usage(int t) const
{
    assert(t >= 0 && t < data_->num_periods());
    int P = data_->num_products();
    int T = data_->num_periods();
    double usage = 0.0;

    for (int p = 0; p < P; ++p) {
        int idx = p * T + t;
        // Production time: assuming 1 unit of capacity per unit produced.
        usage += production_[idx];
        // Setup time.
        if (setup_[idx])
            usage += data_->setup_time(p);
    }
    return usage;
}

bool LotsizingSolution::has_capacity_violation() const
{
    int T = data_->num_periods();
    for (int t = 0; t < T; ++t) {
        if (capacity_usage(t) > data_->capacity(t) + 1e-9)
            return true;
    }
    return false;
}

bool LotsizingSolution::has_demand_violation() const
{
    int P = data_->num_products();
    int T = data_->num_periods();
    for (int p = 0; p < P; ++p) {
        for (int t = 0; t < T; ++t) {
            if (inventory_[p * T + t] < -1e-9)
                return true;
        }
    }
    return false;
}

double LotsizingSolution::total_backlog() const
{
    int P = data_->num_products();
    int T = data_->num_periods();
    double backlog = 0.0;
    for (int p = 0; p < P; ++p) {
        for (int t = 0; t < T; ++t) {
            double inv = inventory_[p * T + t];
            if (inv < 0.0)
                backlog -= inv;
        }
    }
    return backlog;
}

// ---------------------------------------------------------------------------
//  Internal
// ---------------------------------------------------------------------------

void LotsizingSolution::recompute_inventory_(int p, int from)
{
    int T = data_->num_periods();

    for (int t = from; t < T; ++t) {
        int idx = p * T + t;

        // Gross demand = external demand + dependent demand from BOM parents.
        double gross_demand = data_->demand(p, t);
        // For multi-level: dependent demand is handled at the planning level,
        // not here. The solution stores net production; BOM requirements are
        // accounted for in construction heuristics and operators.

        double prev_inv = (t > 0) ? inventory_[p * T + t - 1] : 0.0;
        inventory_[idx] = prev_inv + production_[idx] - gross_demand;
    }
}

} // namespace coso
