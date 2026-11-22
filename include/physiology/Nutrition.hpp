#pragma once

#include "physiology/NeedMeter.hpp"
#include "physiology/Starvation.hpp"

#include <limits>

namespace colony::physiology {

struct Nutrition {
    static constexpr double kHungerFillSeconds = 30.0 * 60.0;
    NeedMeter hunger;
    Starvation starvation;
    bool emergency = false;

    [[nodiscard]] bool hungry() const { return hunger.urgent; }
    [[nodiscard]] bool starving() const { return hunger.level >= NeedMeter::kMaximum; }
    [[nodiscard]] bool strengthChanging() const { return starvation.level > 0 || starving(); }

    bool setHungerLevel(double level) {
        if (!hunger.setLevel(level)) return false;
        hunger.refreshEmergency(emergency);
        return true;
    }

    void eat(double hungerReduction) { setHungerLevel(hunger.level - hungerReduction); }

    void advance(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0.0) return;
        const double untilStarving = (NeedMeter::kMaximum - hunger.level) * kHungerFillSeconds / NeedMeter::kMaximum;
        const double fedSeconds = std::min(seconds, std::max(0.0, untilStarving));
        starvation.advance(seconds - fedSeconds, fedSeconds);
        hunger.advance(seconds, kHungerFillSeconds);
        hunger.refreshEmergency(emergency);
    }

    [[nodiscard]] double nextEventSeconds() const {
        constexpr double hungerRate = NeedMeter::kMaximum / kHungerFillSeconds;
        if (starving()) {
            constexpr double starvationRate = NeedMeter::kMaximum / Starvation::kDeathSeconds;
            return starvation.lethal() ? std::numeric_limits<double>::infinity() :
                (NeedMeter::kMaximum - starvation.level) / starvationRate;
        }
        const double threshold = !hungry() && hunger.level < NeedMeter::kUrgentThreshold ?
            NeedMeter::kUrgentThreshold : NeedMeter::kMaximum;
        const double untilThreshold = (threshold - hunger.level) / hungerRate;
        return starvation.level > 0 ?
            std::min(untilThreshold,starvation.level * Starvation::kRecoverySeconds / NeedMeter::kMaximum) :
            untilThreshold;
    }
};

} // namespace colony::physiology
