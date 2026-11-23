#pragma once

#include "physiology/Effects.hpp"
#include "physiology/Injuries.hpp"
#include "physiology/Nutrition.hpp"

#include <utility>

namespace colony::physiology {

struct Organism {
    Organism() = default;
    explicit Organism(injury::Body body, injury::Resilience resilience = injury::Resilience::Normal)
        : injuries(body,resilience) {}
    explicit Organism(injury::Profile profile) : injuries(std::move(profile)) {}

    injury::State injuries;
    Nutrition nutrition;

    [[nodiscard]] bool alive() const { return injuries.alive() && !nutrition.starvation.lethal(); }
    [[nodiscard]] const std::vector<ActiveEffect>& effects() const { return effects_; }
    [[nodiscard]] const ActiveEffect* findEffect(std::string_view id) const;
    bool addEffect(ActiveEffect effect);
    bool updateEffect(ActiveEffect effect);
    bool removeEffect(std::string_view id);
    bool applyAnalgesia(std::string id,EffectTarget<injury::Part> target,double seconds,double relief);
    [[nodiscard]] double analgesiaRemainingSeconds(injury::Part part) const;
    void advanceEffects(double seconds, const EffectTick& onTick = {});
    [[nodiscard]] double nextEffectEventSeconds() const;
    void advanceCondition(double seconds,const EffectTick& onTick = {});
    [[nodiscard]] double nextConditionEventSeconds() const;

    [[nodiscard]] double movementMultiplier() const;
    [[nodiscard]] double armMultiplier() const;

private:
    [[nodiscard]] bool validEffect(const ActiveEffect& effect) const;
    [[nodiscard]] double injuryMultiplier(bool injury::ProfilePart::*affected) const;
    std::vector<ActiveEffect> effects_;
};

} // namespace colony::physiology
