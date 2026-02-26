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

void StopCriterion::set_work_limit(WorkUnits const* work,
                                   uint64_t max_work_ticks)
{
    work_ = work;
    max_work_ticks_ = max_work_ticks;
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

    // Deterministic work limit.
    if (work_ && max_work_ticks_ > 0 && work_->ticks() >= max_work_ticks_)
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

uint64_t StopCriterion::work_ticks() const noexcept
{
    return work_ ? work_->ticks() : 0;
}

double StopCriterion::work_units() const noexcept
{
    return work_ ? work_->units() : 0.0;
}

} // namespace coso
