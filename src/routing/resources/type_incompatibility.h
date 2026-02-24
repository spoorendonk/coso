#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace coso {

/// Incompatibility matrix: defines which client type pairs cannot share a route.
///
/// Stored as a symmetric matrix: incompat(a, b) == incompat(b, a).
/// Type ids are non-negative integers. Clients with type -1 have no type
/// and are compatible with everything.
class TypeIncompatibilityMatrix {
public:
    TypeIncompatibilityMatrix() = default;

    /// Construct with the given number of types.  All pairs are initially
    /// compatible.
    explicit TypeIncompatibilityMatrix(int num_types)
        : num_types_(num_types), matrix_(num_types * num_types, false) {}

    /// Mark types a and b as incompatible (symmetric).
    void set_incompatible(int a, int b) {
        assert(a >= 0 && a < num_types_);
        assert(b >= 0 && b < num_types_);
        matrix_[a * num_types_ + b] = true;
        matrix_[b * num_types_ + a] = true;
    }

    /// Check whether types a and b are incompatible.
    [[nodiscard]] bool incompatible(int a, int b) const {
        assert(a >= 0 && a < num_types_);
        assert(b >= 0 && b < num_types_);
        return matrix_[a * num_types_ + b];
    }

    /// Number of distinct client types.
    [[nodiscard]] int num_types() const noexcept { return num_types_; }

private:
    int num_types_ = 0;
    std::vector<bool> matrix_;
};

/// Resource tracking client type incompatibility along a route.
///
/// Each client has an optional type (int, -1 = no type).  An incompatibility
/// matrix defines which type pairs cannot share a route.  The resource state
/// tracks a bitmask of client types present in a subsequence.  The excess
/// function counts the number of incompatible type pairs present.
///
/// This resource supports O(1) merge via bitmask union.  For instances with
/// more than 64 types, a vector-based representation is used.
///
/// Designed for O(1) move evaluation: merge two adjacent subsequence states
/// without re-scanning clients.
struct TypeIncompatibilityResource {
    /// State: set of client types present in a subsequence.
    ///
    /// For up to 64 types, uses a uint64_t bitmask.
    /// For more types, falls back to a vector<bool>.
    struct State {
        uint64_t type_bits = 0;               ///< Bitmask for types 0..63.
        std::vector<bool> type_vec;           ///< For types >= 64.
        int num_types = 0;                    ///< Total number of types in the problem.

        /// Check whether a given type is present in this state.
        [[nodiscard]] bool has_type(int t) const {
            assert(t >= 0 && t < num_types);
            if (t < 64)
                return (type_bits >> t) & 1;
            return type_vec[t - 64];
        }

        /// Set a type as present.
        void add_type(int t) {
            assert(t >= 0 && t < num_types);
            if (t < 64)
                type_bits |= (uint64_t{1} << t);
            else
                type_vec[t - 64] = true;
        }
    };

    /// Initialize state for a single client node.
    ///
    /// @param data     The problem data.
    /// @param client   Client index (0-based among clients).
    /// @param matrix   The incompatibility matrix.
    [[nodiscard]] static State init(ProblemData const& data, int client,
                                    TypeIncompatibilityMatrix const& matrix) {
        assert(client >= 0 && client < data.num_clients());
        int nt = matrix.num_types();
        State s;
        s.num_types = nt;
        if (nt > 64)
            s.type_vec.resize(nt - 64, false);

        int ct = data.client(client).client_type;
        if (ct >= 0 && ct < nt)
            s.add_type(ct);
        return s;
    }

    /// Initialize empty state at depot (no types present).
    [[nodiscard]] static State init_depot(
            TypeIncompatibilityMatrix const& matrix) {
        int nt = matrix.num_types();
        State s;
        s.num_types = nt;
        if (nt > 64)
            s.type_vec.resize(nt - 64, false);
        return s;
    }

    /// Merge two adjacent subsequence states.
    ///
    /// The type set of the combined subsequence is the union of both type sets.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        State result;
        result.num_types = left.num_types;
        result.type_bits = left.type_bits | right.type_bits;

        if (!left.type_vec.empty()) {
            result.type_vec.resize(left.type_vec.size(), false);
            for (size_t i = 0; i < left.type_vec.size(); ++i)
                result.type_vec[i] = left.type_vec[i] || right.type_vec[i];
        }
        return result;
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// Type sets are order-independent, so merge_reverse == merge.
    [[nodiscard]] static State merge_reverse(State const& left,
                                             State const& right) {
        return merge(left, right);
    }

    /// Compute the number of incompatible type pairs present on the route.
    ///
    /// Returns the count of incompatible (type_a, type_b) pairs where both
    /// type_a and type_b are present in the state. Each pair is counted once.
    [[nodiscard]] static int excess(State const& state,
                                    TypeIncompatibilityMatrix const& matrix) {
        int nt = matrix.num_types();
        int count = 0;

        // Collect present types for efficient pairwise check.
        // For small type counts this is fast; for large counts the bitmask
        // iteration is still efficient.
        for (int a = 0; a < nt; ++a) {
            if (!state.has_type(a))
                continue;
            for (int b = a + 1; b < nt; ++b) {
                if (state.has_type(b) && matrix.incompatible(a, b))
                    ++count;
            }
        }
        return count;
    }
};

} // namespace coso
