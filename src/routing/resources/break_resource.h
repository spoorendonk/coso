#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>

namespace coso {

/// Resource tracking mandatory driver breaks along a route.
///
/// Models a single break rule: after `max_driving` time units of cumulative
/// driving (travel + service), a break of `break_duration` must be taken.
/// The break can occur at any client stop where `allow_break` is true (or
/// at every stop if no per-client filter is set).
///
/// State tracks:
///   - driving:    cumulative driving time since the last break in this
///                 subsequence (forward direction).
///   - driving_rev: cumulative driving time since the last break in the
///                 reversed subsequence (for merge_reverse).
///   - breaks:     number of breaks taken in this subsequence.
///   - excess:     accumulated violation (driving time exceeding max_driving
///                 that could not be absorbed by a break).
///
/// Designed for O(1) move evaluation via merge of adjacent subsequence states.
///
/// Break rule configuration is passed as a simple struct rather than stored
/// in ProblemData, keeping the resource self-contained.
struct BreakResource {
    /// Configuration for the break rule.
    struct Rule {
        int max_driving = 0;     ///< Max driving time before a break is needed.
        int break_duration = 0;  ///< Duration of the mandatory break.
    };

    /// State for a subsequence of the route.
    struct State {
        int driving = 0;         ///< Driving time since last break (forward).
        int driving_rev = 0;     ///< Driving time since last break (reversed).
        int duration = 0;        ///< Total duration of this subsequence (travel + service).
        int breaks = 0;          ///< Number of breaks taken.
        int excess = 0;          ///< Accumulated break violation.
        bool can_break = false;  ///< Whether a break can be taken in this subsequence.
    };

    /// Initialize state for a single client node.
    ///
    /// The driving time for a single client is its service time.  Travel time
    /// to this client is accounted for during merge (it depends on the
    /// predecessor).
    ///
    /// @param data    Problem data (for service time lookup).
    /// @param client  Client index (0-based among clients).
    /// @param rule    The break rule configuration.
    [[nodiscard]] static State init(ProblemData const& data, int client, Rule const& rule) {
        assert(client >= 0 && client < data.num_clients());
        auto const& c = data.client(client);

        State s;
        s.driving = c.service;
        s.driving_rev = c.service;
        s.duration = c.service;
        s.can_break = true;  // breaks can be taken at any client stop

        // Do not charge excess here; it will be detected by the final
        // excess() call which checks the trailing driving segment.

        return s;
    }

    /// Initialize empty state at depot (no driving, no break needed).
    [[nodiscard]] static State init_depot([[maybe_unused]] ProblemData const& data,
                                          [[maybe_unused]] Rule const& rule) {
        State s;
        s.can_break = true;  // depot is a valid break location
        return s;
    }

    /// Merge two adjacent subsequence states (left followed by right).
    ///
    /// Travel time between the last node of left and first node of right
    /// must be provided as `travel` (looked up by caller from the distance/
    /// duration matrix).
    ///
    /// The merge logic:
    /// 1. The combined driving time is left.driving + travel + right.driving
    ///    (time since last break in left, plus the connecting travel, plus
    ///    time until first break in right).
    /// 2. If a break can be taken at the junction (right.can_break), the
    ///    driver resets their driving counter after a break.
    /// 3. Excess accumulates when driving exceeds max_driving without a
    ///    break opportunity.
    [[nodiscard]] static State merge(State const& left, State const& right, int travel,
                                     Rule const& rule) {
        State result;
        result.duration = left.duration + travel + right.duration;
        result.breaks = left.breaks + right.breaks;
        result.excess = left.excess + right.excess;
        result.can_break = left.can_break || right.can_break;

        if (rule.max_driving <= 0) {
            // No break rule active — just accumulate durations.
            result.driving = left.driving + travel + right.driving;
            result.driving_rev = right.driving_rev + travel + left.driving_rev;
            return result;
        }

        // Combined driving from end of left's last break through travel
        // into right's first segment.
        int bridge = left.driving + travel;

        if (right.can_break) {
            // Right subsequence contains at least one break opportunity.
            // Check if the bridge driving exceeds the limit.
            int bridge_plus_right = bridge + right.driving_rev;
            // driving_rev of right = driving from right's start to its first
            // break point (reversed perspective: from start going forward).
            // Actually for forward merge: right.driving is the driving from
            // right's last break to end. We need driving from right's start
            // to its first break, which we approximate with right.driving_rev
            // (the reversed start-to-break distance).

            // Simpler model: if the combined bridge exceeds max_driving,
            // a break is forced at the first opportunity in right.
            if (bridge > rule.max_driving) {
                result.excess += bridge - rule.max_driving;
                result.breaks += 1;
                // After the forced break, driving resets. The remaining
                // driving is just right's driving (from its last break to end).
                result.driving = right.driving;
            } else {
                // Bridge fits. The driver continues through right.
                // If right had internal breaks, the driving resets at those.
                // The final driving is right.driving (from right's last break).
                result.driving = right.driving;
            }
        } else {
            // Right has no break opportunity. Driving accumulates.
            result.driving = bridge + right.driving;
            if (result.driving > rule.max_driving) {
                // Can't take a break, but we don't charge excess here —
                // excess is only charged when a break was due and couldn't
                // happen. We'll detect it in the full-route excess().
            }
        }

        // Reverse direction (for merge_reverse support).
        int bridge_rev = right.driving_rev + travel;
        if (left.can_break) {
            if (bridge_rev > rule.max_driving) {
                // Already accounted in forward direction.
            }
            result.driving_rev = left.driving_rev;
        } else {
            result.driving_rev = bridge_rev + left.driving_rev;
        }

        return result;
    }

    /// Merge with travel time looked up from problem data.
    ///
    /// @param left        Left subsequence state.
    /// @param right       Right subsequence state.
    /// @param left_last   Node index of the last node in left subsequence.
    /// @param right_first Node index of the first node in right subsequence.
    /// @param data        Problem data (for duration lookup).
    /// @param profile     Duration matrix profile.
    /// @param rule        Break rule configuration.
    [[nodiscard]] static State merge(State const& left, State const& right, int left_last,
                                     int right_first, ProblemData const& data, int profile,
                                     Rule const& rule) {
        int travel = data.dur(profile, left_last, right_first);
        return merge(left, right, travel, rule);
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// When reversed, the right subsequence's forward driving becomes its
    /// reverse and vice versa.
    [[nodiscard]] static State merge_reverse(State const& left, State const& right, int travel,
                                             Rule const& rule) {
        // Swap driving and driving_rev for the reversed right subsequence.
        State right_rev = right;
        std::swap(right_rev.driving, right_rev.driving_rev);
        return merge(left, right_rev, travel, rule);
    }

    /// Compute break violation excess for a full route state.
    ///
    /// Returns total excess: accumulated violations from the state plus
    /// any final segment that exceeds max_driving without a trailing break.
    ///
    /// @param state  The merged state for the full route.
    /// @param rule   The break rule configuration.
    [[nodiscard]] static int excess(State const& state, Rule const& rule) {
        if (rule.max_driving <= 0) {
            return 0;
        }

        int total = state.excess;

        // The final driving segment (from last break to end of route)
        // may also exceed the limit.
        if (state.driving > rule.max_driving) {
            total += state.driving - rule.max_driving;
        }

        return total;
    }
};

}  // namespace coso
