#pragma once
#include <algorithm>
#include <cmath>

namespace colony {

// Simulation-time consequences of an unmet food need.
struct Starvation {
    static constexpr double kDeathSeconds = 10.0 * 60.0;
    static constexpr double kRecoverySeconds = 5.0 * 60.0;
    double level = 0.0;

    void advance(double starvingSeconds, double fedSeconds) {
        if (!std::isfinite(starvingSeconds) || !std::isfinite(fedSeconds) || starvingSeconds<0 || fedSeconds<0) return;
        // Nourished time precedes starving time when hunger reaches its cap within a step.
        level=std::max(0.0,level-100.0*std::min(fedSeconds/kRecoverySeconds,1.0));
        level=std::min(100.0,level+100.0*std::min(starvingSeconds/kDeathSeconds,1.0));
        if (level>=100.0-1e-9) level=100.0;
    }
    [[nodiscard]] bool lethal() const { return level>=100.0; }
    [[nodiscard]] double activityMultiplier() const { return 1.0-0.5*std::clamp(level/100.0,0.0,1.0); }
};
} // namespace colony
