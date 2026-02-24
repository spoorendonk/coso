#pragma once

#include "assignment/constraints/constraint.h"

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

namespace coso {

/// DFA (Deterministic Finite Automaton) definition for shift pattern rules.
///
/// The alphabet consists of shift-type IDs (0, 1, 2, ...) plus a special
/// symbol for "off" (represented as -1 in the schedule).  States are
/// numbered 0 .. num_states-1.
///
/// Transition table: transitions[state][symbol] = next_state.
/// A transition to -1 (or absent entry) means "no valid transition" and
/// counts as a violation for every remaining day.
struct DFA {
    int num_states       = 0;
    int start_state      = 0;
    std::vector<int> accepting_states;

    /// Transition table indexed by (state * alphabet_size + symbol_index).
    /// Symbol mapping: off (-1) maps to index 0, shift 0 maps to 1, etc.
    /// So alphabet_size = num_shift_types + 1.
    int alphabet_size = 0;
    std::vector<int> transitions;  ///< Flat array: num_states * alphabet_size.

    /// Look up the next state.  Returns -1 if no transition exists.
    [[nodiscard]] int next(int state, int symbol_index) const noexcept
    {
        if (state < 0 || state >= num_states
            || symbol_index < 0 || symbol_index >= alphabet_size)
            return -1;
        return transitions[state * alphabet_size + symbol_index];
    }

    /// Check if a state is accepting.
    [[nodiscard]] bool is_accepting(int state) const noexcept
    {
        for (int s : accepting_states)
            if (s == state)
                return true;
        return false;
    }
};

// --------------------------------------------------------------------------- //
//  DFA builders — common shift pattern rules                                   //
// --------------------------------------------------------------------------- //

/// Build a DFA that rejects sequences with more than `max_consec` consecutive
/// occurrences of `shift_type`.  All other transitions are self-loops on
/// state 0 (the reset/accepting state).
///
/// States: 0 (accepting, no recent match), 1..max_consec (accepting, 1..N
/// consecutive matches), max_consec+1 (rejecting sink — too many).
///
/// `num_shift_types` is the total number of shift types in the problem.
[[nodiscard]] DFA build_max_consecutive_dfa(
    int shift_type, int max_consec, int num_shift_types);

/// Build a DFA that rejects any occurrence of the given forbidden pattern
/// (a sequence of shift-type IDs, where -1 means "off").
///
/// Uses a simple prefix-matching approach: states 0..len track how many
/// symbols of the pattern have been matched so far.  State `len` is the
/// rejecting sink.  All states except `len` are accepting.
[[nodiscard]] DFA build_forbidden_pattern_dfa(
    std::vector<int> const& pattern, int num_shift_types);

// --------------------------------------------------------------------------- //
//  AutomatonConstraint                                                         //
// --------------------------------------------------------------------------- //

/// DFA-based constraint for enforcing shift pattern rules per employee.
///
/// Each employee's shift sequence (over the horizon) is fed through the DFA.
/// If the DFA ends in a non-accepting state, or enters the rejecting sink
/// during processing, violations are counted.
///
/// Violation counting: each day that the DFA is in a non-accepting state
/// contributes one penalty.  This gives a graduated cost that guides the
/// search toward feasibility.
class AutomatonConstraint final : public Constraint {
public:
    /// Construct with a DFA and per-violation penalty.
    explicit AutomatonConstraint(DFA dfa, int penalty = 10000)
        : dfa_(std::move(dfa)), penalty_(penalty)
    {
    }

    [[nodiscard]] int evaluate(
        AssignmentData const& data,
        std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(
        AssignmentData const& data,
        std::vector<std::vector<int>> const& schedule,
        AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "Automaton"; }

    /// Access the underlying DFA (for inspection/testing).
    [[nodiscard]] DFA const& dfa() const noexcept { return dfa_; }

private:
    DFA dfa_;
    int penalty_;

    /// Convert a schedule shift value to a DFA symbol index.
    /// off (-1) -> 0, shift 0 -> 1, shift 1 -> 2, etc.
    [[nodiscard]] static int to_symbol(int shift) noexcept
    {
        return shift + 1;
    }

    /// Count violations for a single employee row by running the DFA.
    [[nodiscard]] int employee_cost(
        std::vector<int> const& row, int horizon) const;
};

} // namespace coso
