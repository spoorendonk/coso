#include "routing/operators/insert_optional.h"

#include <cassert>

namespace coso {

// ===========================================================================
//  InsertOptional
// ===========================================================================

bool InsertOptional::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                    ProblemData const& data) {
    best_delta_ = 0;
    client_ = -1;
    route_ = -1;
    pos_ = -1;

    auto unassigned = sol.unassigned();
    if (unassigned.empty()) {
        return false;
    }

    for (int c : unassigned) {
        // Only consider optional clients.
        if (data.client(c).required) {
            continue;
        }

        // Try inserting into every route at every position.
        for (int r = 0; r < sol.num_routes(); ++r) {
            auto const& route = sol.route(r);

            for (int p = 0; p <= route.size(); ++p) {
                int64_t delta = eval.eval_insert_cost(route, p, c);

                if (delta < best_delta_) {
                    best_delta_ = delta;
                    client_ = c;
                    route_ = r;
                    pos_ = p;
                }
            }
        }
    }

    return best_delta_ < 0;
}

void InsertOptional::apply(Solution& sol) const {
    assert(client_ >= 0 && route_ >= 0 && pos_ >= 0);
    sol.insert_client(route_, pos_, client_);
}

// ===========================================================================
//  RemoveOptional
// ===========================================================================

bool RemoveOptional::find_best_move(Solution const& sol, CostEvaluator const& eval,
                                    ProblemData const& data) {
    best_delta_ = 0;
    route_ = -1;
    pos_ = -1;

    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);

        for (int p = 0; p < route.size(); ++p) {
            int c = route.client(p);

            // Only consider optional clients.
            if (data.client(c).required) {
                continue;
            }

            int64_t delta = eval.eval_remove_cost(route, p);

            if (delta < best_delta_) {
                best_delta_ = delta;
                route_ = r;
                pos_ = p;
            }
        }
    }

    return best_delta_ < 0;
}

void RemoveOptional::apply(Solution& sol) const {
    assert(route_ >= 0 && pos_ >= 0);
    sol.remove_client(route_, pos_);
}

}  // namespace coso
