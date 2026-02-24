#include "lotsizing/lotsizing_operators.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace coso {

// ---------------------------------------------------------------------------
//  Evaluation helpers
// ---------------------------------------------------------------------------

namespace {

/// Compute the cost delta when shifting `quantity` of product `p` from
/// period `from` to period `to`.
double compute_shift_delta(LotsizingData const& data,
                           LotsizingSolution const& sol,
                           int p, int from, int to, double quantity)
{
    double delta = 0.0;

    // Setup cost changes.
    double remaining_from = sol.production(p, from) - quantity;
    bool had_setup_from = sol.setup(p, from);
    bool will_have_setup_from = (remaining_from > 1e-9);
    bool had_setup_to = sol.setup(p, to);
    bool will_have_setup_to = true;  // will produce in `to`

    if (had_setup_from && !will_have_setup_from)
        delta -= data.setup_cost(p);  // save a setup
    if (!had_setup_to && will_have_setup_to)
        delta += data.setup_cost(p);  // new setup

    // Holding cost changes: shifting production earlier (to < from) increases
    // holding in [to, from); shifting later (to > from) decreases holding
    // in [from, to) but may cause backlog.
    double h = data.holding_cost(p);
    if (to < from) {
        // Producing earlier: carry inventory for (from - to) extra periods.
        delta += h * quantity * (from - to);
    } else {
        // Producing later: reduce inventory for (to - from) periods.
        delta -= h * quantity * (to - from);
    }

    // Production cost: same total production, so no variable cost change.

    return delta;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  Evaluation
// ---------------------------------------------------------------------------

ShiftProduction evaluate_shift(LotsizingSolution const& sol,
                               int product, int from_period,
                               int to_period, double quantity)
{
    ShiftProduction shift;
    shift.product     = product;
    shift.from_period = from_period;
    shift.to_period   = to_period;
    shift.quantity    = quantity;
    shift.delta = compute_shift_delta(
        sol.data(), sol, product, from_period, to_period, quantity);
    return shift;
}

MergeSetups evaluate_merge(LotsizingSolution const& sol,
                           int product, int source_period,
                           int target_period)
{
    MergeSetups merge;
    merge.product       = product;
    merge.source_period = source_period;
    merge.target_period = target_period;

    double qty = sol.production(product, source_period);
    merge.delta = compute_shift_delta(
        sol.data(), sol, product, source_period, target_period, qty);

    return merge;
}

SplitLot evaluate_split(LotsizingSolution const& sol,
                        int product, int from_period,
                        int to_period, double quantity)
{
    SplitLot split;
    split.product     = product;
    split.from_period = from_period;
    split.to_period   = to_period;
    split.quantity    = quantity;
    split.delta = compute_shift_delta(
        sol.data(), sol, product, from_period, to_period, quantity);
    return split;
}

// ---------------------------------------------------------------------------
//  Feasibility
// ---------------------------------------------------------------------------

bool is_feasible(LotsizingSolution const& sol, ShiftProduction const& shift)
{
    auto const& data = sol.data();
    int T = data.num_periods();

    // Check we have enough production in from_period.
    if (sol.production(shift.product, shift.from_period) < shift.quantity - 1e-9)
        return false;

    // Check capacity in to_period.
    double cap_usage = sol.capacity_usage(shift.to_period);
    double added = shift.quantity;
    if (!sol.setup(shift.product, shift.to_period))
        added += data.setup_time(shift.product);
    // Remove freed capacity from from_period is handled in the to_period check.
    if (cap_usage + added > data.capacity(shift.to_period) + 1e-9)
        return false;

    // Check no demand violation: if shifting production later, ensure
    // inventory stays non-negative in the gap periods.
    if (shift.to_period > shift.from_period) {
        for (int t = shift.from_period; t < shift.to_period; ++t) {
            double inv = sol.inventory(shift.product, t);
            if (inv - shift.quantity < -1e-9)
                return false;
        }
    }

    return true;
}

bool is_feasible(LotsizingSolution const& sol, MergeSetups const& merge)
{
    ShiftProduction as_shift;
    as_shift.product     = merge.product;
    as_shift.from_period = merge.source_period;
    as_shift.to_period   = merge.target_period;
    as_shift.quantity    = sol.production(merge.product, merge.source_period);
    return is_feasible(sol, as_shift);
}

bool is_feasible(LotsizingSolution const& sol, SplitLot const& split)
{
    ShiftProduction as_shift;
    as_shift.product     = split.product;
    as_shift.from_period = split.from_period;
    as_shift.to_period   = split.to_period;
    as_shift.quantity    = split.quantity;
    return is_feasible(sol, as_shift);
}

// ---------------------------------------------------------------------------
//  Application
// ---------------------------------------------------------------------------

void apply(LotsizingSolution& sol, ShiftProduction const& shift)
{
    sol.add_production(shift.product, shift.from_period, -shift.quantity);
    sol.add_production(shift.product, shift.to_period, shift.quantity);
}

void apply(LotsizingSolution& sol, MergeSetups const& merge)
{
    double qty = sol.production(merge.product, merge.source_period);
    sol.set_production(merge.product, merge.source_period, 0.0);
    sol.add_production(merge.product, merge.target_period, qty);
}

void apply(LotsizingSolution& sol, SplitLot const& split)
{
    sol.add_production(split.product, split.from_period, -split.quantity);
    sol.add_production(split.product, split.to_period, split.quantity);
}

// ---------------------------------------------------------------------------
//  Enumeration
// ---------------------------------------------------------------------------

std::vector<ShiftProduction> enumerate_shifts(LotsizingSolution const& sol)
{
    std::vector<ShiftProduction> moves;
    auto const& data = sol.data();
    int P = data.num_products();
    int T = data.num_periods();

    for (int p = 0; p < P; ++p) {
        for (int from = 0; from < T; ++from) {
            double qty = sol.production(p, from);
            if (qty < 1e-9)
                continue;

            // Try shifting to adjacent periods.
            for (int to : {from - 1, from + 1}) {
                if (to < 0 || to >= T)
                    continue;

                auto shift = evaluate_shift(sol, p, from, to, qty);
                if (is_feasible(sol, shift))
                    moves.push_back(shift);

                // Also try partial shift (half the lot).
                if (qty > 1.0 + 1e-9) {
                    double half = std::floor(qty / 2.0);
                    auto partial = evaluate_shift(sol, p, from, to, half);
                    if (is_feasible(sol, partial))
                        moves.push_back(partial);
                }
            }
        }
    }
    return moves;
}

std::vector<MergeSetups> enumerate_merges(LotsizingSolution const& sol)
{
    std::vector<MergeSetups> moves;
    auto const& data = sol.data();
    int P = data.num_products();
    int T = data.num_periods();

    for (int p = 0; p < P; ++p) {
        for (int src = 0; src < T; ++src) {
            if (!sol.setup(p, src))
                continue;

            // Try merging into adjacent periods.
            for (int tgt : {src - 1, src + 1}) {
                if (tgt < 0 || tgt >= T)
                    continue;
                if (!sol.setup(p, tgt))
                    continue;  // Target must already have a setup.

                auto merge = evaluate_merge(sol, p, src, tgt);
                if (is_feasible(sol, merge))
                    moves.push_back(merge);
            }
        }
    }
    return moves;
}

std::vector<SplitLot> enumerate_splits(LotsizingSolution const& sol)
{
    std::vector<SplitLot> moves;
    auto const& data = sol.data();
    int P = data.num_products();
    int T = data.num_periods();

    // Find overloaded periods and try to split lots into adjacent periods.
    for (int from = 0; from < T; ++from) {
        if (sol.remaining_capacity(from) >= -1e-9)
            continue;  // not overloaded

        for (int p = 0; p < P; ++p) {
            double qty = sol.production(p, from);
            if (qty < 1.0 + 1e-9)
                continue;

            for (int to : {from - 1, from + 1}) {
                if (to < 0 || to >= T)
                    continue;
                if (sol.remaining_capacity(to) < 1.0 - 1e-9)
                    continue;  // target also full

                // Split enough to resolve overload or fill target.
                double overload = -sol.remaining_capacity(from);
                double available = sol.remaining_capacity(to);
                if (!sol.setup(p, to))
                    available -= data.setup_time(p);
                if (available < 1e-9)
                    continue;

                double split_qty = std::min({qty - 1.0, overload, available});
                if (split_qty < 1e-9)
                    continue;

                auto split = evaluate_split(sol, p, from, to, split_qty);
                if (is_feasible(sol, split))
                    moves.push_back(split);
            }
        }
    }
    return moves;
}

} // namespace coso
