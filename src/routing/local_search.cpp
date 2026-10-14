#include "routing/local_search.h"

#include "routing/operators/exchange.h"
#include "routing/operators/swap_star.h"
#include "search/score_assert.h"
#include "search/stop_criterion.h"

namespace coso {

LocalSearch::LocalSearch(ProblemData const& data) : data_(&data) {}

void LocalSearch::run(Solution& sol, CostEvaluator const& eval, StopCriterion* stop) {
    last_num_moves_ = 0;
    last_num_iters_ = 0;

    // First-improvement descent: try operators in order.  When any finds
    // an improving move, apply it and restart from the first operator.
    // Stop when no operator finds any improving move.
    Exchange10 op10;
    Exchange11 op11;
    Exchange20 op20;
    SwapTails op_st;
    SwapStar op_ss;

    // Apply an operator's stored move.  In debug builds, check that the delta
    // it accepted the move on is the change in penalised cost (#221).
    auto apply = [&](auto const& op, char const* name) {
#ifndef NDEBUG
        int64_t before = sol.cost(eval);
        op.apply(sol);
        debug::assert_cost_delta(op.best_delta(), sol.cost(eval) - before, name);
#else
        (void)name;
        op.apply(sol);
#endif
        ++last_num_moves_;
    };

    bool improved = true;
    while (improved) {
        if (stop && stop->should_stop()) {
            break;
        }
        improved = false;
        ++last_num_iters_;

        // Try Exchange(1,0) — relocate.
        if (op10.find_best_move(sol, eval, *data_)) {
            apply(op10, "Exchange10");
            improved = true;
            continue;
        }

        // Try Exchange(1,1) — swap.
        if (op11.find_best_move(sol, eval, *data_)) {
            apply(op11, "Exchange11");
            improved = true;
            continue;
        }

        // Try Exchange(2,0) — relocate pair.
        if (op20.find_best_move(sol, eval, *data_)) {
            apply(op20, "Exchange20");
            improved = true;
            continue;
        }

        // Try SwapTails — inter-route 2-opt.
        if (op_st.find_best_move(sol, eval, *data_)) {
            apply(op_st, "SwapTails");
            improved = true;
            continue;
        }

        // SWAP* is significantly more expensive than the other operators.
        // Under a stop criterion, skip it to avoid consuming the remaining
        // budget in a single neighbourhood scan.
        if (!stop && op_ss.find_best_move(sol, eval, *data_)) {
            apply(op_ss, "SwapStar");
            improved = true;
            continue;
        }
    }
}

}  // namespace coso
