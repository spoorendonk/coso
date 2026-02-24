#pragma once

#include "lotsizing/lotsizing_solution.h"

namespace coso {

/// Lot-for-lot heuristic: produce exactly the net demand in each period.
/// Simple but usually results in many setups.
[[nodiscard]] LotsizingSolution lot_for_lot(LotsizingData const& data);

/// Silver-Meal heuristic: minimize average cost per period by
/// consolidating future demands into the current lot until the
/// per-period cost starts increasing.
[[nodiscard]] LotsizingSolution silver_meal(LotsizingData const& data);

/// Part-period balancing: consolidate demands until cumulative holding
/// cost approximately equals the setup cost.
[[nodiscard]] LotsizingSolution part_period_balancing(LotsizingData const& data);

} // namespace coso
