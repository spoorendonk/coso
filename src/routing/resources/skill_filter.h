#pragma once

#include "routing/problem_data.h"

#include <bit>
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace coso {

/// Resource enforcing vehicle-client skill compatibility.
///
/// Each client may require a set of skills (e.g., "cold_chain", "hazmat").
/// Each vehicle type provides a set of skills.  A route is feasible only if
/// the vehicle's skills are a superset of the union of all client skills on
/// the route.
///
/// Internally, skill strings are mapped to bit positions in a 64-bit mask
/// for efficient set operations.  The state at any point is the union
/// (bitwise OR) of required skills across clients in the subsequence.
///
/// The excess function counts how many skills are required by clients but
/// not provided by the vehicle.
struct SkillFilter {
    /// Precomputed skill data for an instance: maps skill strings to bit
    /// positions and stores per-client / per-vehicle-type bitmasks.
    struct SkillData {
        /// Map from skill name to bit index (0..num_skills-1).
        std::unordered_map<std::string, int> skill_index;
        int num_skills = 0;

        /// Per-client required skill bitmask (indexed by client index).
        std::vector<uint64_t> client_mask;

        /// Per-vehicle-type provided skill bitmask (indexed by vtype index).
        std::vector<uint64_t> vehicle_mask;
    };

    /// Build skill data from problem data.  Call once during setup.
    ///
    /// Assigns a unique bit to each distinct skill string appearing in any
    /// client or vehicle type.  Supports up to 64 distinct skills.
    [[nodiscard]] static SkillData build_skill_data(ProblemData const& data) {
        SkillData sd;

        // Collect all unique skill names.
        auto get_or_assign = [&](std::string const& name) -> int {
            auto [it, inserted] = sd.skill_index.emplace(name, sd.num_skills);
            if (inserted) {
                assert(sd.num_skills < 64 && "SkillFilter supports at most 64 distinct skills");
                ++sd.num_skills;
            }
            return it->second;
        };

        // Index all skills from clients.
        for (int c = 0; c < data.num_clients(); ++c) {
            for (auto const& s : data.client(c).skills) {
                get_or_assign(s);
            }
        }

        // Index all skills from vehicle types.
        for (int v = 0; v < data.num_vehicle_types(); ++v) {
            for (auto const& s : data.vehicle_type(v).skills) {
                get_or_assign(s);
            }
        }

        // Build client masks.
        sd.client_mask.resize(data.num_clients(), 0);
        for (int c = 0; c < data.num_clients(); ++c) {
            uint64_t mask = 0;
            for (auto const& s : data.client(c).skills) {
                mask |= uint64_t{1} << sd.skill_index.at(s);
            }
            sd.client_mask[c] = mask;
        }

        // Build vehicle type masks.
        sd.vehicle_mask.resize(data.num_vehicle_types(), 0);
        for (int v = 0; v < data.num_vehicle_types(); ++v) {
            uint64_t mask = 0;
            for (auto const& s : data.vehicle_type(v).skills) {
                mask |= uint64_t{1} << sd.skill_index.at(s);
            }
            sd.vehicle_mask[v] = mask;
        }

        return sd;
    }

    /// State: the union of required skills across clients in a subsequence.
    struct State {
        uint64_t required = 0;  ///< Bitmask of skills required by clients.
    };

    /// Initialize state for a single client node.
    ///
    /// @param sd      Precomputed skill data.
    /// @param client  Client index (0-based among clients).
    [[nodiscard]] static State init(SkillData const& sd, int client) {
        assert(client >= 0 && client < static_cast<int>(sd.client_mask.size()));
        return {sd.client_mask[client]};
    }

    /// Initialize empty state at depot (no skills required).
    [[nodiscard]] static State init_depot() { return {0}; }

    /// Merge two adjacent subsequence states.
    ///
    /// The merged state requires the union of both subsequences' skills.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        return {left.required | right.required};
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// For SkillFilter, direction does not matter: the set of required
    /// skills is the same regardless of visit order.
    [[nodiscard]] static State merge_reverse(State const& left, State const& right) {
        return merge(left, right);
    }

    /// Compute skill excess: number of skills required but not provided.
    ///
    /// @param state  The merged state for the full route.
    /// @param sd     Precomputed skill data.
    /// @param vtype  Vehicle type index.
    /// @return Number of missing skills (0 = feasible).
    [[nodiscard]] static int excess(State const& state, SkillData const& sd, int vtype) {
        assert(vtype >= 0 && vtype < static_cast<int>(sd.vehicle_mask.size()));
        uint64_t missing = state.required & ~sd.vehicle_mask[vtype];
        return std::popcount(missing);
    }

    /// Convenience: check whether a vehicle type can serve this route.
    [[nodiscard]] static bool feasible(State const& state, SkillData const& sd, int vtype) {
        return excess(state, sd, vtype) == 0;
    }
};

}  // namespace coso
