#pragma once

#include "physiology/Anatomy.hpp"
#include "physiology/BloodVolume.hpp"

#include <vector>

namespace colony::injury {

enum class Body { Humanoid = 0, Quadruped = 1 };
enum class Resilience { Normal = 0, Robust = 1 };
enum class Part { Leg = 0, Arm = 1, Torso = 2, Head = 3 };
enum class Severity { Mild, Serious, Critical };
enum class WoundKind { Impact, Open };

struct Wound {
    Part part;
    Severity severity;
    double restRemaining;
    bool treated = false;
    double damage = 0;
    double openDamage = 0;
    [[nodiscard]] physiology::EffectTarget<Part> target() const { return part; }
    [[nodiscard]] double recoveryRate() const { return treated ? 1.0 : .1; }
    [[nodiscard]] double recoverySeconds() const { return restRemaining/recoveryRate(); }
    [[nodiscard]] double bloodLossRate() const { return treated ? 0 : openDamage * .5; }
};

[[nodiscard]] double bedRestDuration(Severity severity);
inline constexpr double seriousDamage = 12;
inline constexpr double criticalDamage = 24;
inline constexpr double maximumDamage = 256;
[[nodiscard]] double severityDamage(Severity severity);
[[nodiscard]] Severity damageSeverity(double damage);

class State {
public:
    static constexpr int lethalTrauma = 6;
    static constexpr int incapacitatingTrauma = 4;
    static constexpr int quadrupedLethalTrauma = 3;
    static constexpr int quadrupedIncapacitatingTrauma = 2;
    static constexpr int robustLethalTrauma = 6;
    static constexpr int robustIncapacitatingTrauma = 4;
    static constexpr int robustLegIncapacitatingTrauma = 3;
    static constexpr double humanoidInjuryChance = .65;
    State() : State(Body::Humanoid) {}
    explicit State(Body body, Resilience resilience = Resilience::Normal);

    [[nodiscard]] Body body() const { return body_; }
    [[nodiscard]] Resilience resilience() const { return resilience_; }
    [[nodiscard]] const physiology::Anatomy<Part>& anatomy() const;
    [[nodiscard]] const std::vector<Wound>& wounds() const { return wounds_; }
    [[nodiscard]] int trauma() const { return trauma_; }
    [[nodiscard]] double injuryChance() const { return body_==Body::Humanoid ? humanoidInjuryChance : .85; }
    [[nodiscard]] int lethalThreshold() const { return resilience_==Resilience::Robust ? robustLethalTrauma :
        body_==Body::Humanoid ? lethalTrauma : quadrupedLethalTrauma; }
    [[nodiscard]] int incapacitationThreshold() const { return resilience_==Resilience::Robust ? robustIncapacitatingTrauma :
        body_==Body::Humanoid ? incapacitatingTrauma : quadrupedIncapacitatingTrauma; }
    [[nodiscard]] bool alive() const { return trauma_ < lethalThreshold() && !blood_.depleted(); }
    [[nodiscard]] bool canWalk() const;
    [[nodiscard]] bool downed() const { return alive() && !canWalk(); }
    [[nodiscard]] double movementMultiplier() const { return canWalk() ? multiplier(Part::Leg) : 0.0; }
    [[nodiscard]] double armMultiplier() const { return multiplier(Part::Arm); }
    [[nodiscard]] Part hitPart(double roll) const;
    [[nodiscard]] double bloodVolume() const { return blood_.current(); }
    [[nodiscard]] double maximumBloodVolume() const { return blood_.capacity(); }
    [[nodiscard]] double bloodLossRate() const;
    [[nodiscard]] double nextBloodEventSeconds() const;
    bool restoreBloodVolume(double amount) { return blood_.restore(amount); }
    void advanceBlood(double seconds);
    bool applyDamage(Part part, double damage, WoundKind kind = WoundKind::Impact);
    // Explicit wounds retain independent recovery timers when restoring older saves.
    bool record(Part part, Severity severity);
    bool record(Part part, Severity severity, double restRemaining, bool treated = false, double damage = 0);
    bool restoreWound(const Wound& wound);
    [[nodiscard]] bool needsTreatment() const;
    // Treat one wound, preferring higher severity and then greater damage.
    void treat();
    void restInBed(double seconds);
    [[nodiscard]] double nextRecoverySeconds() const;

private:
    [[nodiscard]] double multiplier(Part part) const;
    void refreshTrauma();
    Body body_;
    Resilience resilience_;
    physiology::BloodVolume blood_;
    std::vector<Wound> wounds_;
    int trauma_ = 0;
};

} // namespace colony::injury
