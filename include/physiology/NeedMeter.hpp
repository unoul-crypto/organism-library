#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace colony {

// High values mean an unmet need. Hysteresis keeps an urgent need active
// while the actor is still recovering.
struct NeedMeter {
    static constexpr double kMaximum = 100.0;
    static constexpr double kUrgentThreshold = 80.0;
    static constexpr double kRecoveryThreshold = 40.0;
    static constexpr double kEmergencyRecoveryThreshold = 50.0;
    double level = 0.0;
    bool urgent = false;

    NeedMeter() = default;
    explicit NeedMeter(double recoveryThreshold)
        : recoveryThreshold_(recoveryThreshold) {}

    void refreshUrgency() {
        if (level >= kUrgentThreshold) urgent = true;
        else if (level <= recoveryThreshold_) urgent = false;
    }

    void refreshEmergency(bool& emergency) {
        if (level >= kMaximum) emergency = true;
        else if (level <= kEmergencyRecoveryThreshold && emergency) {
            emergency = false;
            urgent = false;
        }
    }

    bool setLevel(double value) {
        if (!std::isfinite(value)) return false;
        level = std::clamp(value, 0.0, kMaximum);
        for (const double threshold : {0.0, recoveryThreshold_, kEmergencyRecoveryThreshold, kUrgentThreshold, kMaximum})
            if (std::abs(level - threshold) <= 1e-9) level = threshold;
        refreshUrgency();
        return true;
    }

    void advance(double deltaSeconds, double fillSeconds) {
        if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0 &&
            std::isfinite(fillSeconds) && fillSeconds > 0.0)
            setLevel(level + std::min(kMaximum, kMaximum * (deltaSeconds / fillSeconds)));
    }

private:
    double recoveryThreshold_ = kRecoveryThreshold;
};

} // namespace colony
