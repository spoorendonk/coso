#include "search/acceptance.h"

#include <algorithm>

namespace coso {

// --------------------------------------------------------------------------- //
//  LateAcceptance                                                              //
// --------------------------------------------------------------------------- //

LateAcceptance::LateAcceptance(int list_length)
    : list_length_(list_length)
{
}

void LateAcceptance::init(int64_t cost)
{
    list_.assign(list_length_, cost);
    index_ = 0;
}

bool LateAcceptance::accept(int64_t candidate_cost, int64_t current_cost)
{
    int64_t la_cost = list_[index_];
    return candidate_cost <= la_cost || candidate_cost <= current_cost;
}

void LateAcceptance::iteration(int64_t current_cost)
{
    list_[index_] = current_cost;
    index_ = (index_ + 1) % list_length_;
}

// --------------------------------------------------------------------------- //
//  SimulatedAnnealing                                                          //
// --------------------------------------------------------------------------- //

SimulatedAnnealing::SimulatedAnnealing(double initial_temp, double alpha,
                                       unsigned int seed)
    : initial_temp_(initial_temp),
      alpha_(alpha),
      temp_(initial_temp),
      rng_(seed)
{
}

void SimulatedAnnealing::init([[maybe_unused]] int64_t cost)
{
    temp_ = initial_temp_;
}

bool SimulatedAnnealing::accept(int64_t candidate_cost,
                                int64_t current_cost)
{
    if (candidate_cost <= current_cost)
        return true;

    if (temp_ <= 0.0)
        return false;

    double delta = static_cast<double>(candidate_cost - current_cost);
    double prob = std::exp(-delta / temp_);
    return dist_(rng_) < prob;
}

void SimulatedAnnealing::iteration([[maybe_unused]] int64_t current_cost)
{
    temp_ *= alpha_;
}

// --------------------------------------------------------------------------- //
//  RecordToRecord                                                              //
// --------------------------------------------------------------------------- //

RecordToRecord::RecordToRecord(double initial_threshold, double decay)
    : initial_threshold_(initial_threshold),
      decay_(decay),
      threshold_(initial_threshold),
      best_cost_(0)
{
}

void RecordToRecord::init(int64_t cost)
{
    threshold_ = initial_threshold_;
    best_cost_ = cost;
}

bool RecordToRecord::accept(int64_t candidate_cost,
                            [[maybe_unused]] int64_t current_cost)
{
    if (candidate_cost < best_cost_)
        best_cost_ = candidate_cost;

    return static_cast<double>(candidate_cost)
           <= static_cast<double>(best_cost_) + threshold_;
}

void RecordToRecord::iteration([[maybe_unused]] int64_t current_cost)
{
    threshold_ = std::max(0.0, threshold_ - decay_);
}

// --------------------------------------------------------------------------- //
//  AcceptanceCriterion (variant wrapper)                                        //
// --------------------------------------------------------------------------- //

AcceptanceCriterion::AcceptanceCriterion(LateAcceptance la)
    : impl_(std::move(la)) {}

AcceptanceCriterion::AcceptanceCriterion(SimulatedAnnealing sa)
    : impl_(std::move(sa)) {}

AcceptanceCriterion::AcceptanceCriterion(RecordToRecord rtr)
    : impl_(std::move(rtr)) {}

void AcceptanceCriterion::init(int64_t cost)
{
    std::visit([cost](auto& criterion) { criterion.init(cost); }, impl_);
}

bool AcceptanceCriterion::accept(int64_t candidate_cost,
                                 int64_t current_cost)
{
    return std::visit(
        [candidate_cost, current_cost](auto& criterion) {
            return criterion.accept(candidate_cost, current_cost);
        },
        impl_);
}

void AcceptanceCriterion::iteration(int64_t current_cost)
{
    std::visit(
        [current_cost](auto& criterion) { criterion.iteration(current_cost); },
        impl_);
}

} // namespace coso
