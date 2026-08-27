#include "model/lotsizing_model.h"

#include "common/work_units.h"
#include "lotsizing/construction.h"
#include "lotsizing/lotsizing_data.h"
#include "lotsizing/lotsizing_operators.h"
#include "search/stop_criterion.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace coso {

void LotSizingModel::set_num_periods(int periods) {
    if (periods <= 0) {
        throw std::invalid_argument("LotSizingModel::set_num_periods: periods must be > 0");
    }
    num_periods_ = periods;
    demands_.assign(static_cast<size_t>(num_products_),
                    std::vector<double>(static_cast<size_t>(num_periods_), 0.0));
    capacities_.assign(static_cast<size_t>(num_periods_), 0.0);
}

int LotSizingModel::add_product(double setup_cost, double setup_time, double unit_production_cost,
                                double holding_cost) {
    int idx = num_products_++;
    products_.push_back(ProductEntry{
        .setup_cost = setup_cost,
        .setup_time = setup_time,
        .unit_production_cost = unit_production_cost,
        .holding_cost = holding_cost,
    });

    if (num_periods_ > 0) {
        demands_.push_back(std::vector<double>(static_cast<size_t>(num_periods_), 0.0));
    } else {
        demands_.push_back({});
    }

    return idx;
}

void LotSizingModel::set_demand(int product, int period, double demand) {
    if (product < 0 || product >= num_products_) {
        throw std::out_of_range("LotSizingModel::set_demand: invalid product index");
    }
    if (period < 0 || period >= num_periods_) {
        throw std::out_of_range("LotSizingModel::set_demand: invalid period index");
    }
    demands_[product][period] = demand;
}

void LotSizingModel::set_capacity(int period, double capacity) {
    if (period < 0 || period >= num_periods_) {
        throw std::out_of_range("LotSizingModel::set_capacity: invalid period index");
    }
    capacities_[period] = capacity;
}

void LotSizingModel::add_bom(int parent, int child, double quantity) {
    if (parent < 0 || parent >= num_products_ || child < 0 || child >= num_products_) {
        throw std::out_of_range("LotSizingModel::add_bom: invalid product index");
    }
    if (parent == child) {
        throw std::invalid_argument("LotSizingModel::add_bom: parent and child must differ");
    }
    bom_.push_back({parent, child, quantity});
}

Result LotSizingModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    if (num_periods_ <= 0 || num_products_ <= 0) {
        return {};
    }

    LotsizingData::Builder builder;
    builder.set_num_periods(num_periods_);
    work.count(1);

    for (auto const& p : products_) {
        builder.add_product(p.setup_cost, p.setup_time, p.unit_production_cost, p.holding_cost);
        work.count(1);
    }
    for (int p = 0; p < num_products_; ++p) {
        for (int t = 0; t < num_periods_; ++t) {
            builder.set_demand(p, t, demands_[p][t]);
            work.count(1);
        }
    }
    for (int t = 0; t < num_periods_; ++t) {
        builder.set_capacity(t, capacities_[t]);
        work.count(1);
    }
    for (auto const& e : bom_) {
        builder.add_bom(e.parent, e.child, e.quantity);
        work.count(1);
    }

    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    LotsizingData data = builder.build();
    work.count(static_cast<uint64_t>(data.num_products()) +
               static_cast<uint64_t>(data.num_periods()));

    LotsizingSolution best = lot_for_lot(data);
    work.count(static_cast<uint64_t>(data.num_products()) * data.num_periods());

    auto consider = [&](LotsizingSolution const& candidate) {
        if (candidate.feasible() && !best.feasible()) {
            best = candidate;
            return;
        }
        if (candidate.feasible() == best.feasible() && candidate.cost() + 1e-9 < best.cost()) {
            best = candidate;
        }
    };

    if (!stop.should_stop()) {
        consider(silver_meal(data));
        work.count(static_cast<uint64_t>(data.num_products()) * data.num_periods());
    }
    if (!stop.should_stop()) {
        consider(part_period_balancing(data));
        work.count(static_cast<uint64_t>(data.num_products()) * data.num_periods());
    }

    int iterations = 0;
    while (!stop.should_stop()) {
        double best_delta = -1e-9;
        enum class MoveKind { None, Shift, Merge, Split };
        MoveKind kind = MoveKind::None;
        ShiftProduction best_shift;
        MergeSetups best_merge;
        SplitLot best_split;

        auto shifts = enumerate_shifts(best);
        work.count(static_cast<uint64_t>(shifts.size()));
        for (auto const& mv : shifts) {
            if (mv.delta < best_delta && is_feasible(best, mv)) {
                best_delta = mv.delta;
                kind = MoveKind::Shift;
                best_shift = mv;
            }
        }

        auto merges = enumerate_merges(best);
        work.count(static_cast<uint64_t>(merges.size()));
        for (auto const& mv : merges) {
            if (mv.delta < best_delta && is_feasible(best, mv)) {
                best_delta = mv.delta;
                kind = MoveKind::Merge;
                best_merge = mv;
            }
        }

        auto splits = enumerate_splits(best);
        work.count(static_cast<uint64_t>(splits.size()));
        for (auto const& mv : splits) {
            if (mv.delta < best_delta && is_feasible(best, mv)) {
                best_delta = mv.delta;
                kind = MoveKind::Split;
                best_split = mv;
            }
        }

        if (kind == MoveKind::None) {
            break;
        }

        if (kind == MoveKind::Shift) {
            apply(best, best_shift);
        } else if (kind == MoveKind::Merge) {
            apply(best, best_merge);
        } else {
            apply(best, best_split);
        }

        ++iterations;
        work.count(3);
    }

    Result result;
    result.feasible_ = best.feasible();
    result.cost_ = best.cost();
    result.iterations_ = iterations;
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    result.production_quantities_.assign(
        static_cast<size_t>(data.num_products()),
        std::vector<double>(static_cast<size_t>(data.num_periods()), 0.0));
    result.inventory_levels_.assign(
        static_cast<size_t>(data.num_products()),
        std::vector<double>(static_cast<size_t>(data.num_periods()), 0.0));

    for (int p = 0; p < data.num_products(); ++p) {
        for (int t = 0; t < data.num_periods(); ++t) {
            result.production_quantities_[p][t] = best.production(p, t);
            result.inventory_levels_[p][t] = best.inventory(p, t);
        }
    }

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

}  // namespace coso
