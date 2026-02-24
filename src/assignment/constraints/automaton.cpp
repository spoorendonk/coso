#include "assignment/constraints/automaton.h"

#include <algorithm>

namespace coso {

// --------------------------------------------------------------------------- //
//  DFA builders                                                                //
// --------------------------------------------------------------------------- //

DFA build_max_consecutive_dfa(int shift_type, int max_consec, int num_shift_types)
{
    // Alphabet: index 0 = off, index 1..num_shift_types = shift types.
    int const alpha = num_shift_types + 1;
    int const target_sym = shift_type + 1;  // Symbol index for the target shift.

    // States: 0..max_consec are accepting (0..N consecutive matches).
    // State max_consec+1 is the rejecting sink.
    int const num_states = max_consec + 2;
    int const sink = max_consec + 1;

    DFA dfa;
    dfa.num_states = num_states;
    dfa.start_state = 0;
    dfa.alphabet_size = alpha;
    dfa.transitions.assign(num_states * alpha, -1);

    // Accepting states: 0 through max_consec.
    for (int s = 0; s <= max_consec; ++s)
        dfa.accepting_states.push_back(s);

    for (int state = 0; state <= max_consec; ++state) {
        for (int sym = 0; sym < alpha; ++sym) {
            if (sym == target_sym) {
                // Matching the target shift: advance the consecutive counter.
                int next = state + 1;
                if (next > max_consec)
                    next = sink;  // Exceeded max -> rejecting sink.
                dfa.transitions[state * alpha + sym] = next;
            } else {
                // Any other symbol resets the counter.
                dfa.transitions[state * alpha + sym] = 0;
            }
        }
    }

    // Sink state: seeing the target shift stays in sink (still consecutive),
    // but any other symbol resets back to state 0.
    for (int sym = 0; sym < alpha; ++sym) {
        if (sym == target_sym)
            dfa.transitions[sink * alpha + sym] = sink;
        else
            dfa.transitions[sink * alpha + sym] = 0;
    }

    return dfa;
}

DFA build_forbidden_pattern_dfa(std::vector<int> const& pattern, int num_shift_types)
{
    int const len = static_cast<int>(pattern.size());
    int const alpha = num_shift_types + 1;

    // States 0..len-1 are accepting (matched 0..len-1 symbols of the pattern).
    // State len is the rejecting sink (full pattern matched = violation).
    int const num_states = len + 1;
    int const sink = len;

    DFA dfa;
    dfa.num_states = num_states;
    dfa.start_state = 0;
    dfa.alphabet_size = alpha;
    dfa.transitions.assign(num_states * alpha, -1);

    // All states except sink are accepting.
    for (int s = 0; s < len; ++s)
        dfa.accepting_states.push_back(s);

    // Convert pattern to symbol indices.
    std::vector<int> pat_sym(len);
    for (int i = 0; i < len; ++i)
        pat_sym[i] = pattern[i] + 1;  // off(-1)->0, shift 0->1, etc.

    // Build transitions using a simplified failure-function approach.
    // For each state (number of matched symbols) and each input symbol,
    // determine the longest suffix of matched symbols that is also a prefix.
    for (int state = 0; state < len; ++state) {
        for (int sym = 0; sym < alpha; ++sym) {
            if (sym == pat_sym[state]) {
                // Extends the current match.
                dfa.transitions[state * alpha + sym] = state + 1;
            } else {
                // Fall back: find the longest proper suffix of
                // pattern[0..state-1] + sym that matches a prefix of pattern.
                // Simple approach: try decreasing lengths.
                int fallback = 0;
                // Build the sequence matched so far + this symbol.
                // We need to find the longest prefix of pattern that is a
                // suffix of pattern[0..state-1] + sym.
                for (int k = std::min(state, len - 1); k >= 1; --k) {
                    // Check if pattern[0..k-1] matches the last k symbols of
                    // pattern[state-k..state-1] + sym.
                    bool match = true;
                    // The last symbol must match.
                    if (pat_sym[k - 1] != sym) {
                        match = false;
                    } else {
                        // Check the rest.
                        for (int j = 0; j < k - 1; ++j) {
                            int pos = state - (k - 1) + j;
                            if (pos < 0 || pat_sym[j] != pat_sym[pos]) {
                                match = false;
                                break;
                            }
                        }
                    }
                    if (match) {
                        fallback = k;
                        break;
                    }
                }
                dfa.transitions[state * alpha + sym] = fallback;
            }
        }
    }

    // Sink state: after a violation, reset to allow detecting further
    // occurrences.  Transitions mirror state 0 (fresh start).
    for (int sym = 0; sym < alpha; ++sym)
        dfa.transitions[sink * alpha + sym] = dfa.transitions[0 * alpha + sym];

    return dfa;
}

// --------------------------------------------------------------------------- //
//  AutomatonConstraint                                                         //
// --------------------------------------------------------------------------- //

int AutomatonConstraint::employee_cost(
    std::vector<int> const& row, int horizon) const
{
    int cost  = 0;
    int state = dfa_.start_state;

    for (int d = 0; d < horizon; ++d) {
        int sym = to_symbol(row[d]);
        state = dfa_.next(state, sym);
        if (state < 0 || !dfa_.is_accepting(state))
            cost += penalty_;
    }
    return cost;
}

int AutomatonConstraint::evaluate(
    AssignmentData const& data,
    std::vector<std::vector<int>> const& schedule) const
{
    int cost = 0;
    int const ne = data.num_employees();
    int const H  = data.horizon;

    for (int e = 0; e < ne; ++e)
        cost += employee_cost(schedule[e], H);

    return cost;
}

int AutomatonConstraint::evaluate_delta(
    AssignmentData const& data,
    std::vector<std::vector<int>> const& schedule,
    AssignmentMove const& move) const
{
    // Only the affected employee's row changes.
    int e = move.employee;
    int H = data.horizon;

    int before = employee_cost(schedule[e], H);

    auto row = schedule[e];
    row[move.day] = move.new_shift;
    int after = employee_cost(row, H);

    return after - before;
}

} // namespace coso
