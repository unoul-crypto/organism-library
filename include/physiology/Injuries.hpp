#pragma once

#include "physiology/InjuryProfile.hpp"
#include "physiology/BloodVolume.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace colony::injury {

enum class Body { Humanoid = 0, Quadruped = 1, Custom = 2 };
enum class Resilience { Normal = 0, Robust = 1 };
enum class WoundKind { Impact, Open };
using WoundId = std::uint64_t;

struct BuiltinProfile {
    Body body;
    Resilience resilience;
};

[[nodiscard]] std::optional<BuiltinProfile> findBuiltinProfile(std::string_view id);

struct Wound;
using WoundRecoveryFactor = std::function<double(const Wound&)>;

struct WoundRecoveryContext {
    double activityMultiplier = 1; // Standard bed rest. Use zero when recovery is inactive.
    std::vector<WoundRecoveryFactor> factors; // Read-only rules evaluated for each wound.
};

struct Wound {
    Part part;
    Severity severity;
    double restRemaining;
    bool treated = false;
    double damage = 0;
    double openDamage = 0;
    double untreatedRecoveryRate = .1;
    double treatedRecoveryRate = 1;
    double lossPerOpenDamage = .5;
    WoundId id = 0;
    [[nodiscard]] physiology::EffectTarget<Part> target() const { return part; }
    [[nodiscard]] double recoveryRate(const WoundRecoveryContext& context = {}) const;
    [[nodiscard]] double recoverySeconds(const WoundRecoveryContext& context = {}) const;
    void advanceRecovery(double seconds,const WoundRecoveryContext& context = {});
    [[nodiscard]] double bloodLossRate() const { return treated ? 0 : openDamage * lossPerOpenDamage; }
    [[nodiscard]] double impairmentMultiplier() const;
};

[[nodiscard]] double bedRestDuration(Severity severity);
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
    explicit State(Profile profile);

    [[nodiscard]] Body body() const { return body_; }
    [[nodiscard]] Resilience resilience() const { return resilience_; }
    [[nodiscard]] const Profile& profile() const { return profile_; }
    [[nodiscard]] const physiology::Anatomy<Part>& anatomy() const { return profile_.anatomy(); }
    [[nodiscard]] const std::vector<Wound>& wounds() const { return wounds_; }
    [[nodiscard]] const Wound* findWound(WoundId id) const;
    [[nodiscard]] WoundId nextWoundId() const { return nextWoundId_; }
    bool restoreNextWoundId(WoundId id);
    [[nodiscard]] bool hasWound(Part part) const;
    [[nodiscard]] int trauma() const { return trauma_; }
    [[nodiscard]] double injuryChance() const { return profile_.definition().trauma.injuryChance; }
    [[nodiscard]] int lethalThreshold() const { return profile_.definition().trauma.lethal; }
    [[nodiscard]] int incapacitationThreshold() const { return profile_.definition().trauma.incapacitating; }
    [[nodiscard]] bool alive() const { return trauma_ < lethalThreshold() && !blood_.depleted(); }
    [[nodiscard]] bool canWalk() const;
    [[nodiscard]] bool downed() const { return alive() && !canWalk(); }
    [[nodiscard]] double movementMultiplier() const;
    [[nodiscard]] double armMultiplier() const;
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
    // Treat only the given wound, if it is present and untreated.
    bool treat(WoundId id);
    void restInBed(double seconds);
    void advanceRecovery(double seconds,const WoundRecoveryContext& context);
    [[nodiscard]] double nextRecoverySeconds(const WoundRecoveryContext& context = {}) const;

private:
    [[nodiscard]] double multiplier(bool ProfilePart::*affected) const;
    [[nodiscard]] Wound configuredWound(Wound wound) const;
    bool appendWound(Wound wound);
    [[nodiscard]] int traumaFromDamage(double damage) const;
    void refreshTrauma();
    Body body_ = Body::Custom;
    Resilience resilience_ = Resilience::Normal;
    Profile profile_;
    physiology::BloodVolume blood_;
    std::vector<Wound> wounds_;
    WoundId nextWoundId_ = 1;
    int trauma_ = 0;
};

} // namespace colony::injury
