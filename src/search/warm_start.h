#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <stdexcept>
#include <vector>

namespace coso {

/// A set of pinned clients that cannot be moved by local search operators.
///
/// Pinned clients are fixed in their current routes and positions.  Operators
/// should check is_pinned() before attempting to relocate, swap, or remove
/// a client.
///
/// Usage:
///   PinSet pins;
///   pins.pin(3);
///   pins.pin(7);
///   if (pins.is_pinned(3)) { /* skip move */ }
class PinSet {
public:
    PinSet() = default;

    /// Construct a PinSet sized for the given number of clients.
    explicit PinSet(int num_clients);

    /// Pin a client (0-based client index).  No-op if already pinned.
    void pin(int client);

    /// Unpin a client.  No-op if not pinned.
    void unpin(int client);

    /// Whether the given client is pinned.
    [[nodiscard]] bool is_pinned(int client) const;

    /// Return the list of all pinned client indices (sorted).
    [[nodiscard]] std::vector<int> const& pinned() const noexcept {
        return pinned_;
    }

    /// Number of pinned clients.
    [[nodiscard]] int size() const noexcept {
        return static_cast<int>(pinned_.size());
    }

    /// Whether any clients are pinned.
    [[nodiscard]] bool empty() const noexcept { return pinned_.empty(); }

    /// Clear all pins.
    void clear();

private:
    std::vector<bool> is_pinned_;  ///< is_pinned_[client] = true if pinned.
    std::vector<int> pinned_;      ///< Sorted list of pinned client indices.
};

/// Build a Solution from user-provided routes (warm start).
///
/// Each inner vector is a sequence of client indices (0-based) representing
/// one route.  The number of routes must not exceed the total number of
/// vehicles in the problem.  Each client must appear exactly once across
/// all routes (unless it is optional and deliberately omitted).
///
/// Validates:
///   - Client indices are in range [0, num_clients).
///   - No client appears more than once.
///   - Number of routes does not exceed total vehicles.
///   - Required clients are all assigned (optional clients may be omitted).
///
/// @param routes  User-provided routes (list of client-index sequences).
/// @param data    The compiled problem data.
/// @param eval    Cost evaluator (for resource recomputation).
/// @return A fully initialized Solution with resource states computed.
/// @throws std::invalid_argument on validation failure.
[[nodiscard]] Solution warm_start(
    std::vector<std::vector<int>> const& routes,
    ProblemData const& data,
    CostEvaluator const& eval);

} // namespace coso
