#pragma once

#include <vector>

namespace coso {

/// Machine calendar: models availability windows for machines.
///
/// Each machine can have a set of availability intervals (e.g., shifts,
/// maintenance windows). During unavailable periods, no operation can
/// be processed on the machine.
///
/// Intervals are stored as sorted, non-overlapping [start, end) pairs.
/// If no intervals are set for a machine, it is assumed always available.
class MachineCalendar {
public:
    /// An availability interval [start, end).
    struct Interval {
        int start;  ///< inclusive
        int end;    ///< exclusive
    };

    /// Construct a calendar for the given number of machines.
    /// All machines are initially always-available (no restrictions).
    explicit MachineCalendar(int num_machines);

    /// Default-construct an empty calendar (0 machines).
    MachineCalendar() = default;

    /// Add an availability window for a machine.
    /// Multiple windows can be added; they will be merged if overlapping.
    void add_available(int machine, int start, int end);

    /// Check whether a machine is available during the entire interval
    /// [start, end).  Returns true if no calendar is set for the machine
    /// (i.e., always available).
    [[nodiscard]] bool available(int machine, int start, int end) const;

    /// Find the next time >= `time` at which the machine becomes available
    /// for at least `duration` consecutive time units.
    /// Returns `time` if the machine is already available.
    /// Returns -1 if no suitable window exists.
    [[nodiscard]] int next_available(int machine, int time,
                                     int duration = 1) const;

    /// Check whether a machine has any calendar restrictions.
    [[nodiscard]] bool has_calendar(int machine) const;

    [[nodiscard]] int num_machines() const noexcept { return num_machines_; }

private:
    int num_machines_ = 0;

    /// Per-machine sorted availability intervals.
    /// Empty vector means always available.
    std::vector<std::vector<Interval>> intervals_;

    /// Merge overlapping intervals for a machine (called after add).
    void merge_intervals(int machine);
};

} // namespace coso
