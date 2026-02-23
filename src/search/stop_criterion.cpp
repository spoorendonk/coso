#include "search/stop_criterion.h"

namespace coso {

StopCriterion::StopCriterion(double time_limit_s)
    : StopCriterion(time_limit_s, 0, 0)
{
}

StopCriterion::StopCriterion(double time_limit_s,
                             int max_iter,
                             int max_no_improve)
    : start_(Clock::now()),
      time_limit_s_(time_limit_s),
      max_iter_(max_iter),
      max_no_improve_(max_no_improve)
{
}

bool StopCriterion::should_stop() const
{
    // Time limit.
    if (time_limit_s_ > 0.0 && elapsed() >= time_limit_s_)
        return true;

    // Max iterations.
    if (max_iter_ > 0 && iter_ >= max_iter_)
        return true;

    // Max iterations without improvement.
    if (max_no_improve_ > 0 && (iter_ - last_improve_iter_) >= max_no_improve_)
        return true;

    return false;
}

void StopCriterion::iteration()
{
    ++iter_;
}

void StopCriterion::improvement()
{
    last_improve_iter_ = iter_;
}

double StopCriterion::elapsed() const
{
    auto now = Clock::now();
    std::chrono::duration<double> diff = now - start_;
    return diff.count();
}

} // namespace coso
