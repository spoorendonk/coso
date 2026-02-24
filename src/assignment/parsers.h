#pragma once

#include "assignment/assignment_data.h"

#include <string>

namespace coso {

// ---------------------------------------------------------------------------
//  schedulingbenchmarks.org NRP format
// ---------------------------------------------------------------------------
//  Sections separated by "SECTION_<NAME>" headers:
//    SECTION_HORIZON        — planning horizon (number of days)
//    SECTION_SHIFTS         — shift type definitions (ID, length, forbidden successors)
//    SECTION_STAFF          — employee definitions (ID, max shifts, max hours, etc.)
//    SECTION_DAYS_OFF       — fixed days off per employee
//    SECTION_SHIFT_ON_REQUESTS  — soft requests for specific shifts
//    SECTION_SHIFT_OFF_REQUESTS — soft requests to avoid specific shifts
//    SECTION_COVER          — daily coverage requirements per shift type

/// Parse a schedulingbenchmarks.org NRP instance from a string.
[[nodiscard]] AssignmentData parse_nrp(const std::string& content);

/// Read a schedulingbenchmarks.org NRP instance from a file.
[[nodiscard]] AssignmentData read_nrp(const std::string& path);

} // namespace coso
