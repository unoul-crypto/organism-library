#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace colony::physiology {

class BloodVolume {
public:
    BloodVolume(double capacity, double incapacitationFraction, double regenerationFractionPerSecond)
        : capacity_(capacity), current_(capacity), incapacitationVolume_(capacity * incapacitationFraction),
          regenerationRate_(capacity * regenerationFractionPerSecond) {
        if (!std::isfinite(capacity_) || capacity_ <= 0 ||
            !std::isfinite(incapacitationVolume_) || incapacitationVolume_ <= 0 || incapacitationVolume_ >= capacity_ ||
            !std::isfinite(regenerationRate_) || regenerationRate_ <= 0)
            throw std::invalid_argument("Invalid blood volume parameters.");
    }

    [[nodiscard]] double capacity() const { return capacity_; }
    [[nodiscard]] double current() const { return current_; }
    [[nodiscard]] bool depleted() const { return current_ == 0; }
    [[nodiscard]] bool incapacitated() const { return current_ < incapacitationVolume_; }

    bool restore(double amount) {
        if (!std::isfinite(amount) || amount < 0 || amount > capacity_) return false;
        current_ = amount;
        return true;
    }

    void advance(double seconds, double lossRate) {
        if ((current_ == capacity_ && lossRate == 0) || depleted() ||
            !std::isfinite(seconds) || seconds <= 0 || !validRate(lossRate)) return;
        const double rate = lossRate > 0 ? -lossRate : regenerationRate_;
        const double previous = current_;
        current_ = std::clamp(current_ + rate * seconds, 0.0, capacity_);
        // Snap threshold roundoff so grouping fixed ticks gives the same transitions.
        const double tolerance = std::min({1e-8,incapacitationVolume_*.01,(capacity_-incapacitationVolume_)*.01});
        if (current_ < tolerance && previous >= tolerance) current_ = 0;
        else if (capacity_ - current_ < tolerance && capacity_ - previous >= tolerance) current_ = capacity_;
        else if (std::abs(current_ - incapacitationVolume_) < tolerance &&
            std::abs(previous - incapacitationVolume_) >= tolerance) current_ = incapacitationVolume_;
    }

    [[nodiscard]] double nextEventSeconds(double lossRate) const {
        constexpr double never = std::numeric_limits<double>::infinity();
        if (depleted() || !validRate(lossRate)) return never;
        const double threshold = incapacitationVolume_;
        if (lossRate > 0) return (incapacitated() ? current_ : current_ - threshold) / lossRate;
        if (current_ == capacity_) return never;
        return ((incapacitated() ? threshold : capacity_) - current_) / regenerationRate_;
    }

private:
    [[nodiscard]] static bool validRate(double rate) { return std::isfinite(rate) && rate >= 0; }

    double capacity_;
    double current_;
    double incapacitationVolume_;
    double regenerationRate_;
};

} // namespace colony::physiology
