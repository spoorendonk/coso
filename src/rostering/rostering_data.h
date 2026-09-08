#pragma once

#include <climits>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coso {

/// Compiled assignment instance data.
///
/// This is the internal representation used by the rostering engine.
/// It is built from the user-facing RosteringModel.
struct RosteringData {
    // -- Shift types ---------------------------------------------------------

    struct ShiftType {
        std::string name;
        int duration_hours = 0;
    };

    std::vector<ShiftType> shift_types;

    [[nodiscard]] int num_shift_types() const noexcept {
        return static_cast<int>(shift_types.size());
    }

    // -- Employees -----------------------------------------------------------

    struct Employee {
        std::string name;
        std::vector<std::string> skills;
        int max_consecutive_days = 5;
    };

    std::vector<Employee> employees;

    [[nodiscard]] int num_employees() const noexcept { return static_cast<int>(employees.size()); }

    // -- Horizon -------------------------------------------------------------

    int horizon = 0;  ///< Planning horizon in days.

    // -- Demand --------------------------------------------------------------

    struct Demand {
        int min_employees = 0;
        int max_employees = INT_MAX;
        std::string required_skill;
    };

    /// Demand indexed by (shift_type, day).
    /// Key: (shift_type_id, day_index).
    std::unordered_map<int64_t, Demand> demand;

    /// Helper to create a key for the demand map.
    static int64_t demand_key(int shift_type, int day) noexcept {
        return (static_cast<int64_t>(shift_type) << 32) |
               static_cast<int64_t>(static_cast<uint32_t>(day));
    }

    /// Look up demand for a given shift type and day. Returns default if absent.
    [[nodiscard]] Demand get_demand(int shift_type, int day) const {
        auto it = demand.find(demand_key(shift_type, day));
        return it != demand.end() ? it->second : Demand{};
    }

    // -- Hard constraints ----------------------------------------------------

    /// Forbidden shift-type sequences.
    std::vector<std::vector<int>> forbidden_sequences;

    // -- Soft constraints / preferences --------------------------------------

    struct Preference {
        int employee;
        int day;
        int shift_type;
        int weight;  ///< Positive = preferred, negative = penalised.
    };

    std::vector<Preference> preferences;

    /// Set of (employee, day) pairs where the employee is unavailable.
    /// Stored as (employee_id << 32 | day).
    std::unordered_set<int64_t> unavailabilities;

    static int64_t unavail_key(int employee, int day) noexcept {
        return (static_cast<int64_t>(employee) << 32) |
               static_cast<int64_t>(static_cast<uint32_t>(day));
    }

    [[nodiscard]] bool is_unavailable(int employee, int day) const {
        return unavailabilities.count(unavail_key(employee, day)) > 0;
    }
};

}  // namespace coso
