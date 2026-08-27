#pragma once

#include <cstdint>

namespace coso {

/// Deterministic work counter used for cross-machine performance measurement.
///
/// The counter tracks abstract "ticks" (integer work operations).  For human
/// reporting and benchmark CSV output, ticks are normalized to work units
/// using a fixed conversion factor.
class WorkUnits {
public:
    static constexpr double kTickToUnit = 1e-6;

    void count(uint64_t ticks) noexcept { ticks_ += ticks; }
    void reset() noexcept { ticks_ = 0; }

    [[nodiscard]] uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] double units() const noexcept {
        return static_cast<double>(ticks_) * kTickToUnit;
    }

    [[nodiscard]] static uint64_t ticks_from_units(double units) noexcept {
        if (units <= 0.0) {
            return 0;
        }
        return static_cast<uint64_t>(units / kTickToUnit);
    }

private:
    uint64_t ticks_ = 0;
};

}  // namespace coso
