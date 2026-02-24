#include "assignment/parsers.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace coso {

namespace {

/// Read entire file into a string.
std::string read_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/// Trim leading and trailing whitespace.
std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Split a string by a delimiter character.
std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        auto t = trim(token);
        if (!t.empty())
            tokens.push_back(std::move(t));
    }
    return tokens;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  schedulingbenchmarks.org NRP format
// ---------------------------------------------------------------------------
//
// The format uses sections delimited by SECTION_XXX headers.
// Example (abbreviated):
//
//   SECTION_HORIZON
//   14
//
//   SECTION_SHIFTS
//   D,Early,480,0,0   (ID, name, length_minutes, ...)
//   N,Night,480,0,1
//
//   SECTION_STAFF
//   Employee0,MaxShifts=...
//
//   SECTION_DAYS_OFF
//   Employee0,0,1
//
//   SECTION_SHIFT_ON_REQUESTS
//   Employee0,3,D,2
//
//   SECTION_SHIFT_OFF_REQUESTS
//   Employee0,5,N,3
//
//   SECTION_COVER
//   0,D,2,1,1      (day, shift_id, requirement, under_weight, over_weight)
//

AssignmentData parse_nrp(const std::string& content)
{
    AssignmentData data;

    std::istringstream in(content);
    std::string line;

    // Maps from string IDs to indices.
    std::unordered_map<std::string, int> shift_id_map;
    std::unordered_map<std::string, int> employee_id_map;

    // Forbidden successor shift pairs (shift_id -> set of successor shift_ids).
    // Stored as indices after all shifts are parsed.
    std::vector<std::pair<std::string, std::vector<std::string>>> forbidden_after;

    enum class Section {
        None,
        Horizon,
        Shifts,
        Staff,
        DaysOff,
        ShiftOnRequests,
        ShiftOffRequests,
        Cover,
    };

    Section section = Section::None;

    while (std::getline(in, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '#') continue;  // Skip comment lines.

        // Detect section headers.
        if (trimmed.starts_with("SECTION_")) {
            if (trimmed == "SECTION_HORIZON")
                section = Section::Horizon;
            else if (trimmed == "SECTION_SHIFTS")
                section = Section::Shifts;
            else if (trimmed == "SECTION_STAFF")
                section = Section::Staff;
            else if (trimmed == "SECTION_DAYS_OFF")
                section = Section::DaysOff;
            else if (trimmed == "SECTION_SHIFT_ON_REQUESTS")
                section = Section::ShiftOnRequests;
            else if (trimmed == "SECTION_SHIFT_OFF_REQUESTS")
                section = Section::ShiftOffRequests;
            else if (trimmed == "SECTION_COVER")
                section = Section::Cover;
            else
                section = Section::None;  // unknown section, skip
            continue;
        }

        switch (section) {
        case Section::Horizon: {
            data.horizon = std::stoi(trimmed);
            break;
        }

        case Section::Shifts: {
            // Format: ShiftID,Duration[,ForbiddenAfterShiftIDs (pipe-separated)]
            auto tokens = split(trimmed, ',');
            if (tokens.size() < 2)
                throw std::runtime_error("NRP parse error: bad SHIFTS line: " + trimmed);

            AssignmentData::ShiftType shift;
            std::string id = tokens[0];

            // Duration field: if value <= 24, treat as hours; otherwise as minutes.
            int duration_val = std::stoi(tokens[1]);
            if (duration_val <= 24) {
                shift.duration_hours = duration_val;
            } else {
                shift.duration_hours = (duration_val + 59) / 60;  // ceil to hours
            }
            shift.name = id;

            int shift_idx = static_cast<int>(data.shift_types.size());
            shift_id_map[id] = shift_idx;
            data.shift_types.push_back(std::move(shift));

            // Forbidden successor shifts (field index 2 if present, pipe-separated).
            if (tokens.size() >= 3 && !tokens[2].empty()) {
                auto forbidden_ids = split(tokens[2], '|');
                forbidden_after.emplace_back(id, std::move(forbidden_ids));
            }
            break;
        }

        case Section::Staff: {
            // Two formats are supported:
            //
            // Named format (key=value):
            //   EmployeeID,MaxShifts=X,MaxTotalMinutes=Y,...
            //
            // Positional format (schedulingbenchmarks.org):
            //   EmployeeID,MaxShifts(e.g. D=14),MaxTotalMinutes,MinTotalMinutes,
            //   MaxConsecutiveShifts,MinConsecutiveShifts,MinConsecutiveDaysOff,
            //   MaxWeekends
            auto tokens = split(trimmed, ',');
            if (tokens.empty())
                throw std::runtime_error("NRP parse error: bad STAFF line: " + trimmed);

            AssignmentData::Employee emp;
            emp.name = tokens[0];

            // Detect positional format: field 2 (index 2) is a plain integer
            // (MaxTotalMinutes) without '=' prefix.
            bool positional = (tokens.size() >= 5
                               && tokens[2].find('=') == std::string::npos);

            if (positional) {
                // Positional: [0]=ID, [1]=MaxShifts, [2]=MaxTotalMinutes,
                //             [3]=MinTotalMinutes, [4]=MaxConsecutiveShifts,
                //             [5]=MinConsecutiveShifts, [6]=MinConsecutiveDaysOff,
                //             [7]=MaxWeekends
                int max_total_minutes = std::stoi(tokens[2]);
                emp.max_hours_per_week = max_total_minutes / 60;
                emp.max_consecutive_days = std::stoi(tokens[4]);
            } else {
                // Named key=value format.
                for (size_t i = 1; i < tokens.size(); ++i) {
                    auto eq = tokens[i].find('=');
                    if (eq == std::string::npos) continue;
                    auto key = tokens[i].substr(0, eq);
                    auto val = tokens[i].substr(eq + 1);

                    if (key == "MaxTotalMinutes") {
                        int minutes = std::stoi(val);
                        emp.max_hours_per_week = minutes / 60;
                    } else if (key == "MaxConsecutiveShifts") {
                        emp.max_consecutive_days = std::stoi(val);
                    }
                }
            }

            int emp_idx = static_cast<int>(data.employees.size());
            employee_id_map[emp.name] = emp_idx;
            data.employees.push_back(std::move(emp));
            break;
        }

        case Section::DaysOff: {
            // Format: EmployeeID,Day1,Day2,...
            auto tokens = split(trimmed, ',');
            if (tokens.size() < 2) break;

            auto it = employee_id_map.find(tokens[0]);
            if (it == employee_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown employee in DAYS_OFF: " + tokens[0]);

            int emp = it->second;
            for (size_t i = 1; i < tokens.size(); ++i) {
                int day = std::stoi(tokens[i]);
                data.unavailabilities.insert(
                    AssignmentData::unavail_key(emp, day));
            }
            break;
        }

        case Section::ShiftOnRequests: {
            // Format: EmployeeID,Day,ShiftID,Weight
            auto tokens = split(trimmed, ',');
            if (tokens.size() < 4)
                throw std::runtime_error(
                    "NRP parse error: bad SHIFT_ON_REQUESTS line: " + trimmed);

            auto emp_it = employee_id_map.find(tokens[0]);
            if (emp_it == employee_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown employee: " + tokens[0]);
            auto shift_it = shift_id_map.find(tokens[2]);
            if (shift_it == shift_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown shift: " + tokens[2]);

            AssignmentData::Preference pref;
            pref.employee   = emp_it->second;
            pref.day        = std::stoi(tokens[1]);
            pref.shift_type = shift_it->second;
            pref.weight     = std::stoi(tokens[3]);  // positive = preferred
            data.preferences.push_back(pref);
            break;
        }

        case Section::ShiftOffRequests: {
            // Format: EmployeeID,Day,ShiftID,Weight
            auto tokens = split(trimmed, ',');
            if (tokens.size() < 4)
                throw std::runtime_error(
                    "NRP parse error: bad SHIFT_OFF_REQUESTS line: " + trimmed);

            auto emp_it = employee_id_map.find(tokens[0]);
            if (emp_it == employee_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown employee: " + tokens[0]);
            auto shift_it = shift_id_map.find(tokens[2]);
            if (shift_it == shift_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown shift: " + tokens[2]);

            AssignmentData::Preference pref;
            pref.employee   = emp_it->second;
            pref.day        = std::stoi(tokens[1]);
            pref.shift_type = shift_it->second;
            pref.weight     = -std::stoi(tokens[3]);  // negative = penalised
            data.preferences.push_back(pref);
            break;
        }

        case Section::Cover: {
            // Format: Day,ShiftID,Requirement,UnderWeight,OverWeight
            auto tokens = split(trimmed, ',');
            if (tokens.size() < 3)
                throw std::runtime_error(
                    "NRP parse error: bad COVER line: " + trimmed);

            auto shift_it = shift_id_map.find(tokens[1]);
            if (shift_it == shift_id_map.end())
                throw std::runtime_error(
                    "NRP parse error: unknown shift in COVER: " + tokens[1]);

            int day        = std::stoi(tokens[0]);
            int shift_type = shift_it->second;
            int required   = std::stoi(tokens[2]);

            AssignmentData::Demand dem;
            dem.min_employees = required;
            data.demand[AssignmentData::demand_key(shift_type, day)] = dem;
            break;
        }

        case Section::None:
            break;
        }
    }

    // Build forbidden sequences from collected data.
    for (auto& [shift_id, forbidden_ids] : forbidden_after) {
        auto src_it = shift_id_map.find(shift_id);
        if (src_it == shift_id_map.end()) continue;

        for (auto& fid : forbidden_ids) {
            auto dst_it = shift_id_map.find(fid);
            if (dst_it == shift_id_map.end()) continue;
            data.forbidden_sequences.push_back({src_it->second, dst_it->second});
        }
    }

    return data;
}

AssignmentData read_nrp(const std::string& path)
{
    return parse_nrp(read_file(path));
}

} // namespace coso
