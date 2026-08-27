#pragma once

#include "assignment/assignment_data.h"
#include "assignment/constraints/constraint.h"

#include <algorithm>
#include <climits>
#include <vector>

namespace coso {

/// Constraint propagation filter for the assignment engine.
///
/// Maintains a domain state per (employee, day) cell: a bitmask of shift types
/// that are still feasible given the current schedule.  The propagation rules
/// prune shift types that would violate hard constraints:
///
///   1. Max consecutive days: if assigning any shift on a day would create a
///      consecutive run exceeding the limit, all shifts are pruned for that cell.
///   2. Min rest: if the rest between a candidate shift and its predecessor or
///      successor shift is insufficient, that candidate shift is pruned.
///   3. Forbidden sequences: if assigning a shift on a day would complete a
///      forbidden sequence, that shift is pruned.
///   4. Demand limits: if a shift/day pair is already at max demand, that shift
///      is pruned for additional employees (unless they already hold it).
///   5. Unavailability: if an employee is unavailable on a day, all shifts are
///      pruned for that cell.
///
/// The filter is designed to be rebuilt cheaply from a schedule.  Call
/// propagate() after constructing or after significant schedule changes.  Then
/// use is_feasible() to test individual assignments or filter_moves() to bulk-
/// prune a list of AssignmentMove candidates.
class CPFilter {
public:
    explicit CPFilter(AssignmentData const& data)
        : data_(data),
          num_shifts_(data.num_shift_types()),
          all_shifts_mask_((1u << data.num_shift_types()) - 1u),
          domains_(data.num_employees(), std::vector<unsigned>(data.horizon, 0)) {}

    /// Run constraint propagation on the given schedule.
    ///
    /// Resets all domains to "all shifts feasible" and then prunes according
    /// to each propagation rule.
    void propagate(std::vector<std::vector<int>> const& schedule) {
        reset_domains();
        prune_unavailabilities();
        prune_max_consecutive(schedule);
        prune_min_rest(schedule);
        prune_forbidden_sequences(schedule);
        prune_demand_limits(schedule);
    }

    /// Check whether assigning `shift` to `employee` on `day` is feasible
    /// according to the propagated domains.
    ///
    /// A shift value of -1 (unassign) is always considered feasible.
    [[nodiscard]] bool is_feasible(int employee, int day, int shift) const {
        if (shift < 0) {
            return true;  // Unassigning is always OK.
        }
        if (shift >= num_shifts_) {
            return false;
        }
        return (domains_[employee][day] & (1u << shift)) != 0;
    }

    /// Remove moves from `moves` whose new_shift is pruned by propagation.
    ///
    /// Moves with new_shift == -1 (unassign) are never removed.
    /// Returns the number of moves removed.
    int filter_moves(std::vector<AssignmentMove>& moves) const {
        auto original_size = static_cast<int>(moves.size());
        moves.erase(std::remove_if(moves.begin(), moves.end(),
                                   [this](AssignmentMove const& m) {
                                       return !is_feasible(m.employee, m.day, m.new_shift);
                                   }),
                    moves.end());
        return original_size - static_cast<int>(moves.size());
    }

    /// Read-only access to the domain of a cell (bitmask of feasible shifts).
    [[nodiscard]] unsigned domain(int employee, int day) const { return domains_[employee][day]; }

    /// Number of feasible shifts for a cell.
    [[nodiscard]] int domain_size(int employee, int day) const {
        return __builtin_popcount(domains_[employee][day]);
    }

private:
    void reset_domains() {
        for (auto& row : domains_) {
            std::fill(row.begin(), row.end(), all_shifts_mask_);
        }
    }

    void prune_unavailabilities() {
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (int e = 0; e < ne; ++e) {
            for (int d = 0; d < H; ++d) {
                if (data_.is_unavailable(e, d)) {
                    domains_[e][d] = 0;
                }
            }
        }
    }

    void prune_max_consecutive(std::vector<std::vector<int>> const& schedule) {
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (int e = 0; e < ne; ++e) {
            int max_consec =
                std::min(data_.max_consecutive_shifts, data_.employees[e].max_consecutive_days);
            if (max_consec >= H) {
                continue;  // Can never violate.
            }

            // For each unassigned day, check if assigning any shift there
            // would create a consecutive run exceeding max_consec.
            for (int d = 0; d < H; ++d) {
                if (schedule[e][d] >= 0) {
                    continue;  // Already assigned, skip.
                }

                // Count consecutive assigned days before d.
                int before = 0;
                for (int b = d - 1; b >= 0 && schedule[e][b] >= 0; --b) {
                    ++before;
                }

                // Count consecutive assigned days after d.
                int after = 0;
                for (int a = d + 1; a < H && schedule[e][a] >= 0; ++a) {
                    ++after;
                }

                // If placing any shift here creates run > max_consec, prune all.
                if (before + 1 + after > max_consec) {
                    domains_[e][d] = 0;
                }
            }
        }
    }

    void prune_min_rest(std::vector<std::vector<int>> const& schedule) {
        if (data_.min_rest_between_shifts <= 0) {
            return;
        }

        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (int e = 0; e < ne; ++e) {
            int min_rest =
                std::max(data_.min_rest_between_shifts, data_.employees[e].min_rest_hours);

            for (int d = 0; d < H; ++d) {
                if (schedule[e][d] >= 0) {
                    continue;  // Already assigned.
                }

                // Check against predecessor shift (day d-1).
                if (d > 0 && schedule[e][d - 1] >= 0) {
                    int prev_shift = schedule[e][d - 1];
                    int end_prev = data_.shift_types[prev_shift].end_hour;

                    for (int s = 0; s < num_shifts_; ++s) {
                        int start_s = data_.shift_types[s].start_hour;
                        int rest = (24 - end_prev) + start_s;
                        if (rest < min_rest) {
                            domains_[e][d] &= ~(1u << s);
                        }
                    }
                }

                // Check against successor shift (day d+1).
                if (d + 1 < H && schedule[e][d + 1] >= 0) {
                    int next_shift = schedule[e][d + 1];
                    int start_next = data_.shift_types[next_shift].start_hour;

                    for (int s = 0; s < num_shifts_; ++s) {
                        int end_s = data_.shift_types[s].end_hour;
                        int rest = (24 - end_s) + start_next;
                        if (rest < min_rest) {
                            domains_[e][d] &= ~(1u << s);
                        }
                    }
                }
            }
        }
    }

    void prune_forbidden_sequences(std::vector<std::vector<int>> const& schedule) {
        if (data_.forbidden_sequences.empty()) {
            return;
        }

        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (auto const& seq : data_.forbidden_sequences) {
            int const len = static_cast<int>(seq.size());
            if (len < 2) {
                continue;
            }

            for (int e = 0; e < ne; ++e) {
                // For each position in the sequence, check if placing a
                // specific shift at an unassigned day completes the sequence.
                for (int start = 0; start + len <= H; ++start) {
                    // Find if there is exactly one unassigned position in
                    // this window and all others match the sequence.
                    int free_pos = -1;
                    bool all_match = true;

                    for (int k = 0; k < len; ++k) {
                        int d = start + k;
                        if (schedule[e][d] < 0) {
                            if (free_pos >= 0) {
                                // More than one free position: can't
                                // complete sequence with one move.
                                all_match = false;
                                break;
                            }
                            free_pos = k;
                        } else if (schedule[e][d] != seq[k]) {
                            all_match = false;
                            break;
                        }
                    }

                    if (all_match && free_pos >= 0) {
                        // Assigning seq[free_pos] at day (start + free_pos)
                        // would complete the forbidden sequence.
                        int d = start + free_pos;
                        int forbidden_shift = seq[free_pos];
                        if (forbidden_shift >= 0 && forbidden_shift < num_shifts_) {
                            domains_[e][d] &= ~(1u << forbidden_shift);
                        }
                    }
                }
            }
        }
    }

    void prune_demand_limits(std::vector<std::vector<int>> const& schedule) {
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        // Count current assignments per (shift, day).
        // Only prune if we know the max is already met.
        for (int s = 0; s < num_shifts_; ++s) {
            for (int d = 0; d < H; ++d) {
                auto dem = data_.get_demand(s, d);
                if (dem.max_employees >= INT_MAX) {
                    continue;  // No cap.
                }

                int count = 0;
                for (int e = 0; e < ne; ++e) {
                    if (schedule[e][d] == s) {
                        ++count;
                    }
                }

                if (count >= dem.max_employees) {
                    // At or over max: prune this shift for all unassigned
                    // employees on this day.
                    for (int e = 0; e < ne; ++e) {
                        if (schedule[e][d] < 0) {
                            domains_[e][d] &= ~(1u << s);
                        }
                    }
                }
            }
        }
    }

    AssignmentData const& data_;
    int num_shifts_;
    unsigned all_shifts_mask_;

    /// domains_[employee][day] is a bitmask: bit s is set if shift s is
    /// feasible for that (employee, day) cell.
    std::vector<std::vector<unsigned>> domains_;
};

}  // namespace coso
