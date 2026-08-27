#pragma once

#include <cassert>
#include <climits>
#include <vector>

namespace coso {

/// Sequence-dependent setup time matrix.
///
/// Stores setup times between pairs of operations (or jobs) on each machine.
/// Used during schedule evaluation: when operation `b` follows operation `a`
/// on a machine, the setup time `setup_time(a, b, machine)` must elapse
/// between the completion of `a` and the start of `b`.
///
/// Internally stored as a flat 3D array: [from][to][machine].
/// Returns 0 for any entry that has not been explicitly set.
class SetupTimeMatrix {
public:
    /// Construct a setup time matrix for the given dimensions.
    ///
    /// @param num_operations  Number of operations (or jobs, depending on usage).
    /// @param num_machines    Number of machines.
    SetupTimeMatrix(int num_operations, int num_machines);

    /// Default-construct an empty matrix (0 operations, 0 machines).
    SetupTimeMatrix() = default;

    /// Set the setup time when switching from operation `from` to `to` on
    /// the given machine.
    void set(int from, int to, int machine, int time);

    /// Set a uniform setup time from `from` to `to` on all machines.
    void set(int from, int to, int time);

    /// Query the setup time from operation `from` to `to` on the given machine.
    /// Returns 0 if not explicitly set.
    [[nodiscard]] int setup_time(int from, int to, int machine) const;

    /// Check whether any non-zero setup times exist.
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    [[nodiscard]] int num_operations() const noexcept { return num_ops_; }
    [[nodiscard]] int num_machines() const noexcept { return num_machines_; }

private:
    int num_ops_ = 0;
    int num_machines_ = 0;

    /// Flat row-major: data_[from * num_ops_ * num_machines_ + to * num_machines_ + machine].
    std::vector<int> data_;

    [[nodiscard]] int index(int from, int to, int machine) const {
        assert(from >= 0 && from < num_ops_);
        assert(to >= 0 && to < num_ops_);
        assert(machine >= 0 && machine < num_machines_);
        return from * num_ops_ * num_machines_ + to * num_machines_ + machine;
    }
};

}  // namespace coso
