#include "routing/local_search.h"

#include "routing/operators/exchange.h"

namespace coso {

LocalSearch::LocalSearch(ProblemData const& data)
    : data_(&data)
{
}

void LocalSearch::run(Solution& sol, CostEvaluator const& eval)
{
    last_num_moves_ = 0;
    last_num_iters_ = 0;

    // First-improvement descent: try operators in order.  When any finds
    // an improving move, apply it and restart from the first operator.
    // Stop when no operator finds any improving move.
    Exchange10 op10;
    Exchange11 op11;
    Exchange20 op20;
    SwapTails  op_st;

    bool improved = true;
    while (improved) {
        improved = false;
        ++last_num_iters_;

        // Try Exchange(1,0) — relocate.
        if (op10.find_best_move(sol, eval, *data_)) {
            op10.apply(sol);
            ++last_num_moves_;
            improved = true;
            continue;
        }

        // Try Exchange(1,1) — swap.
        if (op11.find_best_move(sol, eval, *data_)) {
            op11.apply(sol);
            ++last_num_moves_;
            improved = true;
            continue;
        }

        // Try Exchange(2,0) — relocate pair.
        if (op20.find_best_move(sol, eval, *data_)) {
            op20.apply(sol);
            ++last_num_moves_;
            improved = true;
            continue;
        }

        // Try SwapTails — inter-route 2-opt.
        if (op_st.find_best_move(sol, eval, *data_)) {
            op_st.apply(sol);
            ++last_num_moves_;
            improved = true;
            continue;
        }
    }
}

} // namespace coso
