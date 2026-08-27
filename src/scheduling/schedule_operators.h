#pragma once

#include "scheduling/disjunctive_graph.h"

#include <climits>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Move representations
// ---------------------------------------------------------------------------

/// Move for SwapAdjacentOps: swap positions pos and pos+1 on a machine.
struct SwapAdjacentMove {
    int machine;  ///< machine index
    int pos;      ///< position in the machine sequence (swap pos with pos+1)
};

/// Move for InsertOp: remove the operation at from_pos and insert at to_pos.
struct InsertMove {
    int machine = -1;   ///< machine index
    int from_pos = -1;  ///< position to remove from
    int to_pos = -1;    ///< position to insert at (after removal)
};

/// Move for BlockReverse: reverse operations in [start_pos, end_pos] on a machine.
struct BlockReverseMove {
    int machine = -1;    ///< machine index
    int start_pos = -1;  ///< first position of the block (inclusive)
    int end_pos = -1;    ///< last position of the block (inclusive)
};

// ---------------------------------------------------------------------------
//  SwapAdjacentOps — N1 neighbourhood
// ---------------------------------------------------------------------------

/// Swap two adjacent operations on the same machine in the disjunctive graph.
///
/// This is the simplest neighbourhood (N1): for each machine, each pair of
/// consecutive operations can be swapped.
class SwapAdjacentOps {
public:
    /// Enumerate all valid swap moves for the current graph state.
    [[nodiscard]] static std::vector<SwapAdjacentMove> enumerate(DisjunctiveGraph const& graph);

    /// Apply a swap move: modifies the machine sequence in the graph.
    static void apply(DisjunctiveGraph& graph, SwapAdjacentMove const& move);

    /// Evaluate the makespan after applying the move, without permanently
    /// modifying the graph.  Returns the new makespan, or INT_MAX if the
    /// move creates a cycle.
    [[nodiscard]] static int evaluate(DisjunctiveGraph& graph, SwapAdjacentMove const& move);
};

// ---------------------------------------------------------------------------
//  InsertOp — N5 neighbourhood
// ---------------------------------------------------------------------------

/// Remove an operation from its position on a machine and insert it at
/// another position on the same machine.
///
/// This is the N5 neighbourhood from Nowicki & Smutnicki: for every operation
/// on every machine, try every other position on that machine.
///
/// Note: some moves may create cycles in the disjunctive graph (when the new
/// machine ordering contradicts job precedence constraints).  The evaluate()
/// method detects this and returns INT_MAX for such moves.
class InsertOp {
public:
    /// Enumerate all valid insert moves.
    [[nodiscard]] static std::vector<InsertMove> enumerate(DisjunctiveGraph const& graph);

    /// Apply an insert move.
    static void apply(DisjunctiveGraph& graph, InsertMove const& move);

    /// Evaluate the makespan after applying the move without permanently
    /// modifying the graph.  Returns INT_MAX if the move creates a cycle.
    [[nodiscard]] static int evaluate(DisjunctiveGraph& graph, InsertMove const& move);
};

// ---------------------------------------------------------------------------
//  BlockReverse — N7 neighbourhood
// ---------------------------------------------------------------------------

/// Reverse a contiguous block of operations on a machine.
///
/// The N7 neighbourhood targets critical blocks: maximal sequences of
/// consecutive operations on the same machine that lie on the critical path.
/// Reversing such a block can reduce the makespan.
class BlockReverse {
public:
    /// Find critical blocks: maximal sequences of consecutive critical-path
    /// operations on the same machine.  Returns a list of BlockReverseMoves
    /// (one per critical block of size >= 2).
    [[nodiscard]] static std::vector<BlockReverseMove> enumerate_critical(DisjunctiveGraph& graph);

    /// Enumerate all possible block reversals (not just critical blocks).
    /// For each machine, every sub-sequence of length >= 2 is a candidate.
    [[nodiscard]] static std::vector<BlockReverseMove> enumerate_all(DisjunctiveGraph const& graph);

    /// Apply a block-reverse move.
    static void apply(DisjunctiveGraph& graph, BlockReverseMove const& move);

    /// Evaluate the makespan after applying the move without permanently
    /// modifying the graph.  Returns INT_MAX if the move creates a cycle.
    [[nodiscard]] static int evaluate(DisjunctiveGraph& graph, BlockReverseMove const& move);
};

}  // namespace coso
