#include "rostering/parsers.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace coso {

namespace {

/// Read entire file into a string.
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/// Trim leading and trailing whitespace.
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Split a string by a delimiter character.
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        auto t = trim(token);
        if (!t.empty()) {
            tokens.push_back(std::move(t));
        }
    }
    return tokens;
}

/// Split a string by a delimiter character, keeping empty fields, so a caller
/// can tell "A,,4320" from "A,4320".
std::vector<std::string> split_fields(const std::string& s, char delim) {
    std::vector<std::string> fields;
    std::istringstream iss(s);
    std::string field;
    while (std::getline(iss, field, delim)) {
        fields.push_back(trim(field));
    }
    return fields;
}

/// Parse a whole field as an int, throwing `what` if any of it is not one.
int parse_int(const std::string& field, const std::string& what) {
    std::size_t used = 0;
    int value = 0;
    try {
        value = std::stoi(field, &used);
    } catch (const std::logic_error&) {
        throw std::runtime_error(what);
    }
    if (used != field.size()) {
        throw std::runtime_error(what);
    }
    return value;
}

}  // anonymous namespace

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
//   D,480,           (ShiftID, length in minutes, forbidden successors)
//   N,480,D|N        ("D|N" = neither D nor N may follow N)
//
//   SECTION_STAFF
//   A,D=14,4320,3390,5,2,2,1
//     (ID, MaxShifts per type, MaxTotalMinutes, MinTotalMinutes,
//      MaxConsecutiveShifts, MinConsecutiveShifts, MinConsecutiveDaysOff,
//      MaxWeekends) -- or the key=value dialect, Employee0,MaxShifts=...
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

RosteringData parse_nrp(const std::string& content) {
    RosteringData data;

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
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed[0] == '#') {
            continue;  // Skip comment lines.
        }

        // Detect section headers.
        if (trimmed.starts_with("SECTION_")) {
            if (trimmed == "SECTION_HORIZON") {
                section = Section::Horizon;
            } else if (trimmed == "SECTION_SHIFTS") {
                section = Section::Shifts;
            } else if (trimmed == "SECTION_STAFF") {
                section = Section::Staff;
            } else if (trimmed == "SECTION_DAYS_OFF") {
                section = Section::DaysOff;
            } else if (trimmed == "SECTION_SHIFT_ON_REQUESTS") {
                section = Section::ShiftOnRequests;
            } else if (trimmed == "SECTION_SHIFT_OFF_REQUESTS") {
                section = Section::ShiftOffRequests;
            } else if (trimmed == "SECTION_COVER") {
                section = Section::Cover;
            } else {
                section = Section::None;  // unknown section, skip
            }
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
                if (tokens.size() < 2) {
                    throw std::runtime_error("NRP parse error: bad SHIFTS line: " + trimmed);
                }

                RosteringData::ShiftType shift;
                std::string id = tokens[0];

                // Duration field is "Length in mins" -- no unit guessing.
                shift.duration_minutes = std::stoi(tokens[1]);
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
                // Two dialects, told apart by shape rather than guessed at
                // (#236); a line that is neither is rejected.
                //
                // An ID-only line declares an employee with no bounds.
                //
                // Positional (schedulingbenchmarks.org), exactly eight fields:
                //   ID, MaxShifts (e.g. D=14|N=4), MaxTotalMinutes, MinTotalMinutes,
                //   MaxConsecutiveShifts, MinConsecutiveShifts,
                //   MinConsecutiveDaysOff, MaxWeekends
                // recognised by no '=' from the third field on.
                //
                // Named: ID followed only by Key=Value fields, any subset of
                // the seven keys above, each at most once.
                //
                // MaxShifts, MinConsecutiveShifts and MinConsecutiveDaysOff
                // are read in both dialects and not represented: the #203
                // ruling defers them.
                auto const bad = "NRP parse error: bad STAFF line: " + trimmed;
                auto fields = split_fields(trimmed, ',');
                if (fields.empty() || fields[0].empty()) {
                    throw std::runtime_error(bad);
                }
                for (auto const& f : fields) {
                    if (f.empty()) {
                        throw std::runtime_error(bad + " (empty field)");
                    }
                }

                RosteringData::Employee emp;
                emp.name = fields[0];

                bool const positional =
                    fields.size() >= 3 &&
                    std::none_of(fields.begin() + 2, fields.end(),
                                 [](std::string const& f) { return f.contains('='); });

                if (positional) {
                    if (fields.size() != 8) {
                        throw std::runtime_error(
                            bad + " (expected 8 positional fields or only Key=Value fields, got " +
                            std::to_string(fields.size()) + " fields)");
                    }
                    // MaxShifts, deferred, but checked so a key=value field
                    // cannot pass for it: ShiftID=N|ShiftID=N|...
                    for (auto const& cap : split_fields(fields[1], '|')) {
                        auto eq = cap.find('=');
                        if (eq == std::string::npos || !shift_id_map.contains(cap.substr(0, eq))) {
                            throw std::runtime_error(bad + " (MaxShifts is ShiftID=N|...)");
                        }
                        parse_int(cap.substr(eq + 1), bad);
                    }
                    emp.max_total_minutes = parse_int(fields[2], bad);
                    emp.min_total_minutes = parse_int(fields[3], bad);
                    emp.max_consecutive_days = parse_int(fields[4], bad);
                    parse_int(fields[5], bad);  // MinConsecutiveShifts, deferred
                    parse_int(fields[6], bad);  // MinConsecutiveDaysOff, deferred
                    emp.max_weekends = parse_int(fields[7], bad);
                } else {
                    std::vector<std::string> seen;
                    for (size_t i = 1; i < fields.size(); ++i) {
                        auto eq = fields[i].find('=');
                        if (eq == std::string::npos) {
                            throw std::runtime_error(bad + " (mixes positional and key=value)");
                        }
                        auto key = fields[i].substr(0, eq);
                        auto val = fields[i].substr(eq + 1);
                        if (std::ranges::find(seen, key) != seen.end()) {
                            throw std::runtime_error(
                                std::format("{} (duplicate key {})", bad, key));
                        }
                        seen.push_back(key);

                        if (key == "MaxConsecutiveShifts") {
                            emp.max_consecutive_days = parse_int(val, bad);
                        } else if (key == "MaxTotalMinutes") {
                            emp.max_total_minutes = parse_int(val, bad);
                        } else if (key == "MinTotalMinutes") {
                            emp.min_total_minutes = parse_int(val, bad);
                        } else if (key == "MaxWeekends") {
                            emp.max_weekends = parse_int(val, bad);
                        } else if (key == "MaxShifts" || key == "MinConsecutiveShifts" ||
                                   key == "MinConsecutiveDaysOff") {
                            parse_int(val, bad);  // deferred
                        } else {
                            throw std::runtime_error(std::format("{} (unknown key {})", bad, key));
                        }
                    }
                }

                if (employee_id_map.contains(emp.name)) {
                    throw std::runtime_error("NRP parse error: duplicate employee in STAFF: " +
                                             emp.name);
                }
                int emp_idx = static_cast<int>(data.employees.size());
                employee_id_map[emp.name] = emp_idx;
                data.employees.push_back(std::move(emp));
                break;
            }

            case Section::DaysOff: {
                // Format: EmployeeID,Day1,Day2,...
                auto tokens = split(trimmed, ',');
                if (tokens.size() < 2) {
                    break;
                }

                auto it = employee_id_map.find(tokens[0]);
                if (it == employee_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown employee in DAYS_OFF: " +
                                             tokens[0]);
                }

                int emp = it->second;
                for (size_t i = 1; i < tokens.size(); ++i) {
                    int day = std::stoi(tokens[i]);
                    data.unavailabilities.insert(RosteringData::unavail_key(emp, day));
                }
                break;
            }

            case Section::ShiftOnRequests: {
                // Format: EmployeeID,Day,ShiftID,Weight
                auto tokens = split(trimmed, ',');
                if (tokens.size() < 4) {
                    throw std::runtime_error("NRP parse error: bad SHIFT_ON_REQUESTS line: " +
                                             trimmed);
                }

                auto emp_it = employee_id_map.find(tokens[0]);
                if (emp_it == employee_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown employee: " + tokens[0]);
                }
                auto shift_it = shift_id_map.find(tokens[2]);
                if (shift_it == shift_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown shift: " + tokens[2]);
                }

                RosteringData::Preference pref;
                pref.employee = emp_it->second;
                pref.day = std::stoi(tokens[1]);
                pref.shift_type = shift_it->second;
                pref.weight = std::stoi(tokens[3]);  // positive = preferred
                data.preferences.push_back(pref);
                break;
            }

            case Section::ShiftOffRequests: {
                // Format: EmployeeID,Day,ShiftID,Weight
                auto tokens = split(trimmed, ',');
                if (tokens.size() < 4) {
                    throw std::runtime_error("NRP parse error: bad SHIFT_OFF_REQUESTS line: " +
                                             trimmed);
                }

                auto emp_it = employee_id_map.find(tokens[0]);
                if (emp_it == employee_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown employee: " + tokens[0]);
                }
                auto shift_it = shift_id_map.find(tokens[2]);
                if (shift_it == shift_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown shift: " + tokens[2]);
                }

                RosteringData::Preference pref;
                pref.employee = emp_it->second;
                pref.day = std::stoi(tokens[1]);
                pref.shift_type = shift_it->second;
                pref.weight = -std::stoi(tokens[3]);  // negative = penalised
                data.preferences.push_back(pref);
                break;
            }

            case Section::Cover: {
                // Format: Day,ShiftID,Requirement[,UnderWeight,OverWeight]
                // Empty fields are kept and rejected, so a missing column
                // cannot shift the next one into its place (#236).
                auto const bad = "NRP parse error: bad COVER line: " + trimmed;
                auto tokens = split_fields(trimmed, ',');
                if (tokens.size() < 3 || tokens.size() > 5 ||
                    std::ranges::any_of(tokens, [](auto const& t) { return t.empty(); })) {
                    throw std::runtime_error(bad);
                }

                auto shift_it = shift_id_map.find(tokens[1]);
                if (shift_it == shift_id_map.end()) {
                    throw std::runtime_error("NRP parse error: unknown shift in COVER: " +
                                             tokens[1]);
                }

                int day = parse_int(tokens[0], bad);
                int shift_type = shift_it->second;
                int required = parse_int(tokens[2], bad);

                RosteringData::Demand dem;
                dem.min_employees = required;
                if (tokens.size() > 3) {
                    dem.under_weight = parse_int(tokens[3], bad);
                }
                if (tokens.size() > 4) {
                    // SB-NRP's requirement is a target: over-cover is charged
                    // above the same number under-cover is charged below.
                    dem.max_employees = required;
                    dem.over_weight = parse_int(tokens[4], bad);
                }
                if (!data.demand.emplace(RosteringData::demand_key(shift_type, day), dem).second) {
                    throw std::runtime_error("NRP parse error: duplicate COVER line: " + trimmed);
                }
                break;
            }

            case Section::None:
                break;
        }
    }

    // Build forbidden sequences from collected data.
    for (auto& [shift_id, forbidden_ids] : forbidden_after) {
        auto src_it = shift_id_map.find(shift_id);
        if (src_it == shift_id_map.end()) {
            continue;
        }

        for (auto& fid : forbidden_ids) {
            auto dst_it = shift_id_map.find(fid);
            if (dst_it == shift_id_map.end()) {
                continue;
            }
            data.forbidden_sequences.push_back({src_it->second, dst_it->second});
        }
    }

    return data;
}

RosteringData read_nrp(const std::string& path) {
    return parse_nrp(read_file(path));
}

}  // namespace coso
