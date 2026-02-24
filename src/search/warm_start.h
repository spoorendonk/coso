#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <set>
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

/// Configuration for replanning an existing routing solution.
///
/// Replanning re-optimizes an existing solution with constraints:
///   - Pinned clients stay in their current route and position.
///   - New clients are inserted using cheapest insertion.
///   - Removed clients are taken out of their routes.
///   - Local search runs on the modified solution, skipping moves that
///     would relocate or swap pinned clients.
struct ReplanConfig {
    /// Clients that must stay in their current route/position.
    /// These are not moved during local search.
    std::set<int> pinned_clients;

    /// New clients to insert into the existing routes via cheapest insertion.
    /// These must currently be unassigned in the solution.
    std::vector<int> new_clients;

    /// Clients to remove from the current routes.
    /// Must not overlap with pinned_clients.
    std::vector<int> removed_clients;
};

/// Replan an existing routing solution.
///
/// Given an existing solution and a ReplanConfig:
///   1. Remove specified clients from their routes.
///   2. Insert new clients using cheapest insertion.
///   3. Build a PinSet from pinned_clients.
///   4. Run local search, skipping moves that involve pinned clients.
///
/// @param sol     The solution to modify in place.
/// @param config  Replanning configuration.
/// @param data    The compiled problem data.
/// @param eval    Cost evaluator.
/// @return The PinSet of pinned clients (for inspection/further use).
PinSet replan(Solution& sol,
              ReplanConfig const& config,
              ProblemData const& data,
              CostEvaluator const& eval);

/// Insert a single unassigned client into the solution at the cheapest
/// position across all routes.
///
/// Evaluates insertion at every position in every route and picks the one
/// with the smallest cost increase.  If no feasible insertion exists (all
/// routes are full or constraints prevent it), the client remains unassigned.
///
/// @param sol     The solution to modify.
/// @param client  Client index to insert (must be unassigned).
/// @param data    The compiled problem data.
/// @param eval    Cost evaluator for delta evaluation.
/// @return true if the client was successfully inserted.
bool cheapest_insert(Solution& sol,
                     int client,
                     ProblemData const& data,
                     CostEvaluator const& eval);

/// Run local search on a solution, respecting pinned clients.
///
/// Applies first-improvement descent using Exchange(1,0), Exchange(1,1),
/// Exchange(2,0), and SwapTails operators.  Moves that would relocate or
/// swap a pinned client are skipped.
///
/// @param sol   The solution to optimize in place.
/// @param pins  Set of pinned (immovable) clients.
/// @param data  The compiled problem data.
/// @param eval  Cost evaluator.
void local_search_with_pins(Solution& sol,
                            PinSet const& pins,
                            ProblemData const& data,
                            CostEvaluator const& eval);

} // namespace coso
