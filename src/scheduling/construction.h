#pragma once

#include "scheduling/schedule_data.h"
#include "model/types.h"

namespace coso {

/// Scheduling construction heuristics.
///
/// Each function takes a compiled ScheduleData instance and returns a Result
/// populated with a feasible schedule (machine assignments and start times
/// for every operation, plus the makespan).

/// Serial Generation Scheme (SGS) for RCPSP.
///
/// Schedules operations one by one in precedence-feasible order, using an
/// earliest-start-time priority rule. At each step the operation with the
/// smallest earliest feasible start (respecting both precedence and renewable
/// resource capacity constraints) is selected and scheduled.
[[nodiscard]] Result construct_sgs(ScheduleData const& data);

/// NEH heuristic for flow-shop / job-shop problems.
///
/// 1. Sort jobs by total processing time (descending).
/// 2. Insert jobs one by one into the partial schedule at the position
///    that minimises the resulting makespan.
///
/// Machine assignment uses fixed machines when available; for FJSP operations
/// the machine with shortest processing time is selected.
[[nodiscard]] Result construct_neh(ScheduleData const& data);

/// Dispatching-rule construction (SPT / LPT).
enum class DispatchRule {
    SPT,  ///< Shortest Processing Time first
    LPT,  ///< Longest Processing Time first
};

/// Build a schedule by dispatching ready operations according to the given
/// priority rule. Operations become ready once all predecessors are complete.
/// Ties are broken by operation index.
[[nodiscard]] Result construct_dispatch(ScheduleData const& data,
                                        DispatchRule rule = DispatchRule::SPT);

} // namespace coso
