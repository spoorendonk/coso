#pragma once

#include "lotsizing/lotsizing_solution.h"

#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Move descriptors
// ---------------------------------------------------------------------------

/// Shift production of a product from one period to another.
struct ShiftProduction {
    int product = -1;
    int from_period = -1;
    int to_period = -1;
    double quantity = 0.0;  ///< amount to shift
    double delta = 0.0;     ///< cost delta (negative = improvement)
};

/// Merge setups: move all production of a product from one period into an
/// adjacent period, eliminating a setup.
struct MergeSetups {
    int product = -1;
    int source_period = -1;  ///< period whose setup is eliminated
    int target_period = -1;  ///< period that absorbs production
    double delta = 0.0;
};

/// Split a large lot across two periods to balance capacity usage.
struct SplitLot {
    int product = -1;
    int from_period = -1;
    int to_period = -1;
    double quantity = 0.0;  ///< amount moved to to_period
    double delta = 0.0;
};

// ---------------------------------------------------------------------------
//  Evaluation
// ---------------------------------------------------------------------------

/// Evaluate cost delta for shifting `quantity` of product from `from_period`
/// to `to_period`. Does not modify the solution.
[[nodiscard]] ShiftProduction evaluate_shift(LotsizingSolution const& sol, int product,
                                             int from_period, int to_period, double quantity);

/// Evaluate merging all production of product from source_period into
/// target_period.
[[nodiscard]] MergeSetups evaluate_merge(LotsizingSolution const& sol, int product,
                                         int source_period, int target_period);

/// Evaluate splitting `quantity` from a lot in from_period to to_period.
[[nodiscard]] SplitLot evaluate_split(LotsizingSolution const& sol, int product, int from_period,
                                      int to_period, double quantity);

// ---------------------------------------------------------------------------
//  Feasibility checks
// ---------------------------------------------------------------------------

/// Check if a shift is feasible (capacity in to_period, no negative
/// production in from_period, no demand violation).
[[nodiscard]] bool is_feasible(LotsizingSolution const& sol, ShiftProduction const& shift);

/// Check if a merge is feasible (capacity in target_period).
[[nodiscard]] bool is_feasible(LotsizingSolution const& sol, MergeSetups const& merge);

/// Check if a split is feasible.
[[nodiscard]] bool is_feasible(LotsizingSolution const& sol, SplitLot const& split);

// ---------------------------------------------------------------------------
//  Application
// ---------------------------------------------------------------------------

/// Apply a ShiftProduction move to the solution.
void apply(LotsizingSolution& sol, ShiftProduction const& shift);

/// Apply a MergeSetups move to the solution.
void apply(LotsizingSolution& sol, MergeSetups const& merge);

/// Apply a SplitLot move to the solution.
void apply(LotsizingSolution& sol, SplitLot const& split);

// ---------------------------------------------------------------------------
//  Enumeration
// ---------------------------------------------------------------------------

/// Enumerate all feasible ShiftProduction moves (shifting entire lots
/// or partial lots between adjacent periods).
[[nodiscard]] std::vector<ShiftProduction> enumerate_shifts(LotsizingSolution const& sol);

/// Enumerate all feasible MergeSetups moves.
[[nodiscard]] std::vector<MergeSetups> enumerate_merges(LotsizingSolution const& sol);

/// Enumerate feasible SplitLot moves for overloaded periods.
[[nodiscard]] std::vector<SplitLot> enumerate_splits(LotsizingSolution const& sol);

}  // namespace coso
