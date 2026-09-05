#include "model/packing_model.h"

#include "common/work_units.h"
#include "packing/packing_data.h"
#include "packing/packing_operators.h"
#include "packing/packing_solution.h"
#include "search/stop_criterion.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <stdexcept>

namespace coso {

int PackingModel::add_bin_type(BinTypeParams p) {
    int idx = static_cast<int>(bin_types_.size());

    // Infer or validate dimensionality.
    int dims = static_cast<int>(p.capacity.size());
    if (dims == 0) {
        throw std::invalid_argument("bin type must have at least one capacity dimension");
    }

    if (num_dims_ == 0) {
        num_dims_ = dims;
    } else if (dims != num_dims_) {
        throw std::invalid_argument("bin type capacity has " + std::to_string(dims) +
                                    " dimensions, expected " + std::to_string(num_dims_));
    }

    bin_types_.push_back(std::move(p));
    return idx;
}

int PackingModel::add_item(ItemParams p) {
    int idx = static_cast<int>(items_.size());

    int dims = static_cast<int>(p.size.size());
    if (dims == 0) {
        throw std::invalid_argument("item must have at least one size dimension");
    }

    if (num_dims_ == 0) {
        num_dims_ = dims;
    } else if (dims != num_dims_) {
        throw std::invalid_argument("item size has " + std::to_string(dims) +
                                    " dimensions, expected " + std::to_string(num_dims_));
    }

    items_.push_back(std::move(p));
    return idx;
}

void PackingModel::add_conflict(int item_a, int item_b) {
    int n = static_cast<int>(items_.size());
    if (item_a < 0 || item_a >= n || item_b < 0 || item_b >= n) {
        throw std::out_of_range("conflict item index out of range");
    }
    if (item_a == item_b) {
        throw std::invalid_argument("an item cannot conflict with itself");
    }

    conflicts_.emplace_back(item_a, item_b);
}

Result PackingModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));
    work.count(static_cast<uint64_t>(bin_types_.size()) + static_cast<uint64_t>(items_.size()) +
               static_cast<uint64_t>(conflicts_.size()));
    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    // Validate: need at least one bin type and one item.
    if (bin_types_.empty() || items_.empty()) {
        return {};  // cannot solve without bins/items
    }

    PackingData data = PackingData::build(*this);
    work.count(static_cast<uint64_t>(data.num_items()) +
               static_cast<uint64_t>(data.num_bin_types()));

    PackingSolution sol(data);
    int const N = data.num_items();
    int const D = data.num_dims();

    // FFD-style construction: place largest items first.
    std::vector<int> items(N);
    std::iota(items.begin(), items.end(), 0);
    std::sort(items.begin(), items.end(), [&](int a, int b) {
        int sa = 0, sb = 0;
        for (int d = 0; d < D; ++d) {
            sa += data.item_size(a, d);
            sb += data.item_size(b, d);
        }
        if (sa != sb) {
            return sa > sb;
        }
        return a < b;
    });

    auto assign_unassigned = [&]() {
        bool changed = false;
        for (int item : items) {
            if (sol.item_bin(item) >= 0) {
                continue;
            }

            int best_bin = -1;
            int best_delta = 0;
            for (int b = 0; b < sol.num_bins(); ++b) {
                if (!sol.item_fits(item, b)) {
                    continue;
                }
                int delta = sol.assign_cost_delta(item, b);
                if (best_bin < 0 || delta < best_delta) {
                    best_bin = b;
                    best_delta = delta;
                    if (delta < 0) {
                        break;
                    }
                }
            }
            if (best_bin >= 0) {
                sol.assign(item, best_bin);
                changed = true;
                work.count(1);
            }
        }
        return changed;
    };

    assign_unassigned();
    work.count(static_cast<uint64_t>(N));

    int iterations = 0;
    while (!stop.should_stop()) {
        bool improved = false;

        auto merges = enumerate_merges(sol);
        work.count(static_cast<uint64_t>(merges.size()));
        MergeBins best_merge;
        bool have_merge = false;
        for (auto const& mv : merges) {
            if (!have_merge || mv.delta < best_merge.delta) {
                best_merge = mv;
                have_merge = true;
            }
        }
        if (have_merge && best_merge.delta < 0 && is_feasible(sol, best_merge)) {
            apply(sol, best_merge);
            improved = true;
            work.count(2);
        }

        if (!improved) {
            auto moves = enumerate_moves(sol);
            work.count(static_cast<uint64_t>(moves.size()));
            MoveItem best_move;
            bool have_move = false;
            for (auto const& mv : moves) {
                if (!have_move || mv.delta < best_move.delta) {
                    best_move = mv;
                    have_move = true;
                }
            }
            if (have_move && best_move.delta < 0 && is_feasible(sol, best_move)) {
                apply(sol, best_move);
                improved = true;
                work.count(2);
            }
        }

        // Try to place items that were previously unassigned.
        if (assign_unassigned()) {
            improved = true;
            work.count(1);
        }

        if (!improved) {
            break;
        }
        ++iterations;
    }

    Result result;
    result.feasible_ = sol.feasible();
    result.cost_ = static_cast<double>(sol.cost());
    result.iterations_ = iterations;
    for (int b = 0; b < sol.num_bins(); ++b) {
        if (!sol.bin_items(b).empty()) {
            result.bins_.push_back(sol.bin_items(b));
        }
    }
    for (int i = 0; i < N; ++i) {
        if (sol.item_bin(i) < 0) {
            result.unassigned_.push_back(i);
        }
    }
    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

}  // namespace coso
