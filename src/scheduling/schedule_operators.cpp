#include "scheduling/schedule_operators.h"

#include <algorithm>
#include <cassert>
#include <climits>

namespace coso {

namespace {

/// After applying a move that modifies machine sequences, the resulting
/// graph may contain a cycle (the new machine ordering can contradict
/// fixed job-precedence arcs).  In that case critical_path() returns 0
/// or a nonsensical value because the internal topological sort does not
/// visit all nodes.
///
/// We detect this by checking: if the graph has at least one operation
/// with positive duration, then a valid DAG must have makespan > 0.
/// A makespan of 0 therefore signals a cycle.
int safe_critical_path(DisjunctiveGraph& graph) {
    int ms = graph.critical_path();
    if (ms > 0) {
        return ms;
    }

    // Check if any operation has positive duration.
    for (int i = 0; i < graph.num_operations(); ++i) {
        if (graph.operation(i).duration > 0) {
            return INT_MAX;  // cycle detected
        }
    }
    return 0;  // all zero-duration ops — 0 is correct
}

}  // namespace

// ===========================================================================
//  SwapAdjacentOps — N1 neighbourhood
// ===========================================================================

std::vector<SwapAdjacentMove> SwapAdjacentOps::enumerate(DisjunctiveGraph const& graph) {
    std::vector<SwapAdjacentMove> moves;
    for (int m = 0; m < graph.num_machines(); ++m) {
        auto const& seq = graph.machine_sequence(m);
        for (int p = 0; p + 1 < static_cast<int>(seq.size()); ++p) {
            moves.push_back({.machine = m, .pos = p});
        }
    }
    return moves;
}

void SwapAdjacentOps::apply(DisjunctiveGraph& graph, SwapAdjacentMove const& move) {
    auto seq = graph.machine_sequence(move.machine);
    assert(move.pos >= 0 && move.pos + 1 < static_cast<int>(seq.size()));
    std::swap(seq[move.pos], seq[move.pos + 1]);
    graph.set_sequence(move.machine, seq);
}

int SwapAdjacentOps::evaluate(DisjunctiveGraph& graph, SwapAdjacentMove const& move) {
    auto original_seq = graph.machine_sequence(move.machine);

    apply(graph, move);
    int new_makespan = safe_critical_path(graph);

    // Revert.
    graph.set_sequence(move.machine, original_seq);

    return new_makespan;
}

// ===========================================================================
//  InsertOp — N5 neighbourhood
// ===========================================================================

std::vector<InsertMove> InsertOp::enumerate(DisjunctiveGraph const& graph) {
    std::vector<InsertMove> moves;
    for (int m = 0; m < graph.num_machines(); ++m) {
        auto const& seq = graph.machine_sequence(m);
        int n = static_cast<int>(seq.size());
        for (int from = 0; from < n; ++from) {
            // After removing from_pos, the sequence has n-1 elements.
            // to_pos ranges over 0..(n-2).
            for (int to = 0; to < n - 1; ++to) {
                // Skip no-op moves.
                if (to == from) {
                    continue;
                }
                if (from > 0 && to == from - 1) {
                    continue;
                }
                moves.push_back({.machine = m, .from_pos = from, .to_pos = to});
            }
        }
    }
    return moves;
}

void InsertOp::apply(DisjunctiveGraph& graph, InsertMove const& move) {
    auto seq = graph.machine_sequence(move.machine);
    assert(move.from_pos >= 0 && move.from_pos < static_cast<int>(seq.size()));

    int op = seq[move.from_pos];
    seq.erase(seq.begin() + move.from_pos);

    assert(move.to_pos >= 0 && move.to_pos <= static_cast<int>(seq.size()));
    seq.insert(seq.begin() + move.to_pos, op);

    graph.set_sequence(move.machine, seq);
}

int InsertOp::evaluate(DisjunctiveGraph& graph, InsertMove const& move) {
    auto original_seq = graph.machine_sequence(move.machine);

    apply(graph, move);
    int new_makespan = safe_critical_path(graph);

    graph.set_sequence(move.machine, original_seq);

    return new_makespan;
}

// ===========================================================================
//  BlockReverse — N7 neighbourhood
// ===========================================================================

std::vector<BlockReverseMove> BlockReverse::enumerate_critical(DisjunctiveGraph& graph) {
    auto crit_ops = graph.critical_path_ops();

    std::vector<bool> on_critical(graph.num_operations(), false);
    for (int op : crit_ops) {
        on_critical[op] = true;
    }

    std::vector<BlockReverseMove> moves;
    for (int m = 0; m < graph.num_machines(); ++m) {
        auto const& seq = graph.machine_sequence(m);
        int n = static_cast<int>(seq.size());

        int block_start = -1;
        for (int p = 0; p <= n; ++p) {
            bool is_crit = (p < n) && on_critical[seq[p]];
            if (is_crit) {
                if (block_start < 0) {
                    block_start = p;
                }
            } else {
                if (block_start >= 0 && (p - block_start) >= 2) {
                    moves.push_back({.machine = m, .start_pos = block_start, .end_pos = p - 1});
                }
                block_start = -1;
            }
        }
    }

    return moves;
}

std::vector<BlockReverseMove> BlockReverse::enumerate_all(DisjunctiveGraph const& graph) {
    std::vector<BlockReverseMove> moves;
    for (int m = 0; m < graph.num_machines(); ++m) {
        auto const& seq = graph.machine_sequence(m);
        int n = static_cast<int>(seq.size());
        for (int s = 0; s < n; ++s) {
            for (int e = s + 1; e < n; ++e) {
                moves.push_back({.machine = m, .start_pos = s, .end_pos = e});
            }
        }
    }
    return moves;
}

void BlockReverse::apply(DisjunctiveGraph& graph, BlockReverseMove const& move) {
    auto seq = graph.machine_sequence(move.machine);
    assert(move.start_pos >= 0 && move.end_pos < static_cast<int>(seq.size()) &&
           move.start_pos < move.end_pos);

    std::reverse(seq.begin() + move.start_pos, seq.begin() + move.end_pos + 1);
    graph.set_sequence(move.machine, seq);
}

int BlockReverse::evaluate(DisjunctiveGraph& graph, BlockReverseMove const& move) {
    auto original_seq = graph.machine_sequence(move.machine);

    apply(graph, move);
    int new_makespan = safe_critical_path(graph);

    graph.set_sequence(move.machine, original_seq);

    return new_makespan;
}

}  // namespace coso
