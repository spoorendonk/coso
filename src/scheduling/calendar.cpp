#include "scheduling/calendar.h"

#include <algorithm>
#include <stdexcept>

namespace coso {

MachineCalendar::MachineCalendar(int num_machines)
    : num_machines_(num_machines),
      intervals_(num_machines) {}

void MachineCalendar::add_available(int machine, int start, int end) {
    if (machine < 0 || machine >= num_machines_)
        throw std::out_of_range(
            "MachineCalendar::add_available: invalid machine index");
    if (start >= end)
        return;  // empty interval, ignore

    intervals_[machine].push_back({start, end});
    merge_intervals(machine);
}

bool MachineCalendar::available(int machine, int start, int end) const {
    if (machine < 0 || machine >= num_machines_)
        return false;

    // No calendar restrictions = always available.
    if (intervals_[machine].empty())
        return true;

    if (start >= end)
        return true;

    // Find the interval that could contain [start, end).
    // Intervals are sorted and non-overlapping.
    auto const& ivs = intervals_[machine];

    // Binary search for the first interval whose end > start.
    auto it = std::lower_bound(
        ivs.begin(), ivs.end(), start,
        [](Interval const& iv, int t) { return iv.end <= t; });

    // Check if this interval fully covers [start, end).
    if (it != ivs.end() && it->start <= start && it->end >= end)
        return true;

    return false;
}

int MachineCalendar::next_available(int machine, int time, int duration) const {
    if (machine < 0 || machine >= num_machines_)
        return -1;

    // No calendar restrictions = always available.
    if (intervals_[machine].empty())
        return time;

    if (duration <= 0)
        return time;

    auto const& ivs = intervals_[machine];

    // Binary search for the first interval whose end > time.
    auto it = std::lower_bound(
        ivs.begin(), ivs.end(), time,
        [](Interval const& iv, int t) { return iv.end <= t; });

    for (; it != ivs.end(); ++it) {
        // Earliest we could start within this interval.
        int earliest = std::max(time, it->start);
        if (earliest + duration <= it->end)
            return earliest;
    }

    return -1;  // no suitable window found
}

bool MachineCalendar::has_calendar(int machine) const {
    if (machine < 0 || machine >= num_machines_)
        return false;
    return !intervals_[machine].empty();
}

void MachineCalendar::merge_intervals(int machine) {
    auto& ivs = intervals_[machine];
    if (ivs.size() <= 1)
        return;

    // Sort by start time.
    std::sort(ivs.begin(), ivs.end(),
              [](Interval const& a, Interval const& b) {
                  return a.start < b.start;
              });

    // Merge overlapping/adjacent intervals.
    std::vector<Interval> merged;
    merged.push_back(ivs[0]);

    for (size_t i = 1; i < ivs.size(); ++i) {
        if (ivs[i].start <= merged.back().end) {
            // Overlapping or adjacent — extend.
            merged.back().end = std::max(merged.back().end, ivs[i].end);
        } else {
            merged.push_back(ivs[i]);
        }
    }

    ivs = std::move(merged);
}

} // namespace coso
