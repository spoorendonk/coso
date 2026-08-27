#include "lotsizing/lotsizing_data.h"

#include <cassert>

namespace coso {

LotsizingData LotsizingData::Builder::build() const {
    LotsizingData data;

    int P = static_cast<int>(setup_costs_.size());
    int T = num_periods_;
    assert(T > 0 && "Must set num_periods before building");

    data.num_products_ = P;
    data.num_periods_ = T;

    data.setup_costs_ = setup_costs_;
    data.setup_times_ = setup_times_;
    data.unit_prod_costs_ = unit_prod_costs_;
    data.holding_costs_ = holding_costs_;

    // Copy demands into [P x T] flat array.
    data.demands_.resize(static_cast<size_t>(P) * T, 0.0);
    for (int p = 0; p < P; ++p) {
        for (int t = 0; t < T && t < max_periods_; ++t) {
            if (p * max_periods_ + t < static_cast<int>(demands_.size())) {
                data.demands_[p * T + t] = demands_[p * max_periods_ + t];
            }
        }
    }

    // Copy capacities.
    data.capacities_.resize(T, 0.0);
    for (int t = 0; t < T && t < static_cast<int>(capacities_.size()); ++t) {
        data.capacities_[t] = capacities_[t];
    }

    // Copy BOM and build adjacency lists.
    data.bom_ = bom_;
    data.children_.resize(P);
    data.parents_.resize(P);
    for (auto const& e : bom_) {
        assert(e.parent >= 0 && e.parent < P);
        assert(e.child >= 0 && e.child < P);
        assert(e.parent != e.child);
        data.children_[e.parent].push_back(e);
        data.parents_[e.child].push_back(e);
    }

    return data;
}

}  // namespace coso
