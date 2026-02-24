#include "lotsizing/construction.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Lot-for-lot
// ---------------------------------------------------------------------------

LotsizingSolution lot_for_lot(LotsizingData const& data)
{
    LotsizingSolution sol(data);
    int P = data.num_products();
    int T = data.num_periods();

    for (int p = 0; p < P; ++p) {
        for (int t = 0; t < T; ++t) {
            double d = data.demand(p, t);
            if (d > 0.0)
                sol.set_production(p, t, d);
        }
    }

    return sol;
}

// ---------------------------------------------------------------------------
//  Silver-Meal
// ---------------------------------------------------------------------------

LotsizingSolution silver_meal(LotsizingData const& data)
{
    LotsizingSolution sol(data);
    int P = data.num_products();
    int T = data.num_periods();

    for (int p = 0; p < P; ++p) {
        double h = data.holding_cost(p);
        double s = data.setup_cost(p);

        int t = 0;
        while (t < T) {
            // Skip periods with no demand ahead.
            double d_t = data.demand(p, t);
            if (d_t < 1e-9) {
                ++t;
                continue;
            }

            // Start a new lot in period t.
            double lot_qty = d_t;
            double total_holding = 0.0;
            double total_cost = s; // setup
            int periods_covered = 1;
            double avg_cost = total_cost / periods_covered;

            // Try extending the lot to cover future periods.
            int j = t + 1;
            while (j < T) {
                double d_j = data.demand(p, j);
                if (d_j < 1e-9) {
                    ++j;
                    continue;
                }

                // Holding cost for carrying d_j for (j - t) periods.
                double extra_holding = h * d_j * (j - t);
                double new_total_cost = total_cost + extra_holding;
                int new_periods = periods_covered + 1;
                double new_avg = new_total_cost / new_periods;

                // Silver-Meal: stop when average cost per period increases.
                if (new_avg > avg_cost + 1e-9)
                    break;

                lot_qty += d_j;
                total_holding += extra_holding;
                total_cost = new_total_cost;
                periods_covered = new_periods;
                avg_cost = new_avg;
                ++j;
            }

            sol.set_production(p, t, lot_qty);
            t = j;
        }
    }

    return sol;
}

// ---------------------------------------------------------------------------
//  Part-period balancing
// ---------------------------------------------------------------------------

LotsizingSolution part_period_balancing(LotsizingData const& data)
{
    LotsizingSolution sol(data);
    int P = data.num_products();
    int T = data.num_periods();

    for (int p = 0; p < P; ++p) {
        double h = data.holding_cost(p);
        double s = data.setup_cost(p);

        int t = 0;
        while (t < T) {
            double d_t = data.demand(p, t);
            if (d_t < 1e-9) {
                ++t;
                continue;
            }

            // Start a new lot in period t.
            double lot_qty = d_t;
            double cum_holding = 0.0;

            // Extend lot until cumulative holding cost >= setup cost.
            int j = t + 1;
            while (j < T) {
                double d_j = data.demand(p, j);
                if (d_j < 1e-9) {
                    ++j;
                    continue;
                }

                double extra_holding = h * d_j * (j - t);
                if (cum_holding + extra_holding > s)
                    break;

                lot_qty += d_j;
                cum_holding += extra_holding;
                ++j;
            }

            sol.set_production(p, t, lot_qty);
            t = j;
        }
    }

    return sol;
}

} // namespace coso
