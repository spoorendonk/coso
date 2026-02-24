#include "model/packing_model.h"

#include <chrono>
#include <stdexcept>

namespace coso {

int PackingModel::add_bin_type(BinTypeParams p)
{
    int idx = static_cast<int>(bin_types_.size());

    // Infer or validate dimensionality.
    int dims = static_cast<int>(p.capacity.size());
    if (dims == 0)
        throw std::invalid_argument("bin type must have at least one capacity dimension");

    if (num_dims_ == 0) {
        num_dims_ = dims;
    } else if (dims != num_dims_) {
        throw std::invalid_argument(
            "bin type capacity has " + std::to_string(dims) +
            " dimensions, expected " + std::to_string(num_dims_));
    }

    bin_types_.push_back(std::move(p));
    return idx;
}

int PackingModel::add_item(ItemParams p)
{
    int idx = static_cast<int>(items_.size());

    int dims = static_cast<int>(p.size.size());
    if (dims == 0)
        throw std::invalid_argument("item must have at least one size dimension");

    if (num_dims_ == 0) {
        num_dims_ = dims;
    } else if (dims != num_dims_) {
        throw std::invalid_argument(
            "item size has " + std::to_string(dims) +
            " dimensions, expected " + std::to_string(num_dims_));
    }

    items_.push_back(std::move(p));
    return idx;
}

void PackingModel::add_conflict(int item_a, int item_b)
{
    int n = static_cast<int>(items_.size());
    if (item_a < 0 || item_a >= n || item_b < 0 || item_b >= n)
        throw std::out_of_range("conflict item index out of range");
    if (item_a == item_b)
        throw std::invalid_argument("an item cannot conflict with itself");

    conflicts_.emplace_back(item_a, item_b);
}

void PackingModel::minimize_bins()
{
    minimize_bins_ = true;
}

Result PackingModel::solve(TimeLimit tl)
{
    auto wall_start = std::chrono::steady_clock::now();

    // Validate: need at least one bin type and one item.
    if (bin_types_.empty() || items_.empty()) {
        return {};  // cannot solve without bins/items
    }

    // TODO: Plug in actual packing solver here.
    // For now, return a stub result with feasible_=false.
    Result result;
    result.feasible_ = false;
    result.cost_ = 0.0;
    result.iterations_ = 0;

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(
        wall_end - wall_start).count();

    return result;
}

} // namespace coso
