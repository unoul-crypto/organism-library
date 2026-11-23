#include "physiology/Organism.hpp"
#include "StableIds.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace colony::physiology {
namespace {

double relief(const ActiveEffect& effect) {
    const auto entry = effect.parameters.find("relief");
    if (entry == effect.parameters.end()) return -1;
    if (const auto* value = std::get_if<double>(&entry->second)) return *value;
    if (const auto* value = std::get_if<std::int64_t>(&entry->second)) return static_cast<double>(*value);
    return -1;
}

auto effectWithId(const std::vector<ActiveEffect>& effects, std::string_view id) {
    return std::find_if(effects.begin(),effects.end(),[id](const auto& effect) { return effect.id == id; });
}

} // namespace

bool ActiveEffect::affects(injury::Part part) const {
    if (const auto* single = std::get_if<injury::Part>(&target)) return *single == part;
    if (const auto* parts = std::get_if<std::vector<injury::Part>>(&target))
        return std::find(parts->begin(),parts->end(),part) != parts->end();
    return std::holds_alternative<WholeBody>(target);
}

const ActiveEffect* Organism::findEffect(std::string_view id) const {
    const auto found = effectWithId(effects_,id);
    return found == effects_.end() ? nullptr : &*found;
}

bool Organism::validEffect(const ActiveEffect& effect) const {
    if (!detail::validId(effect.id) || !detail::validId(effect.typeId) || effect.target.valueless_by_exception() ||
        !injuries.anatomy().supports(effect.target) ||
        (effect.remainingSeconds && (!std::isfinite(*effect.remainingSeconds) || *effect.remainingSeconds <= 0)))
        return false;
    for (const auto& entry : effect.parameters) {
        if (!detail::validId(entry.first) || entry.second.valueless_by_exception()) return false;
        const auto* value = std::get_if<double>(&entry.second);
        if (value && !std::isfinite(*value)) return false;
    }
    if (effect.typeId == analgesiaEffectType) {
        const double amount = relief(effect);
        return effect.remainingSeconds && amount >= 0 && amount <= 1;
    }
    return true;
}

bool Organism::addEffect(ActiveEffect effect) {
    if (!validEffect(effect) || findEffect(effect.id)) return false;
    effects_.push_back(std::move(effect));
    return true;
}

bool Organism::updateEffect(ActiveEffect effect) {
    const auto found = effectWithId(effects_,effect.id);
    if (found == effects_.end() || found->typeId != effect.typeId || !validEffect(effect)) return false;
    effects_[static_cast<std::size_t>(found-effects_.cbegin())] = std::move(effect);
    return true;
}

bool Organism::removeEffect(std::string_view id) {
    const auto found = effectWithId(effects_,id);
    if (found == effects_.end()) return false;
    effects_.erase(found);
    return true;
}

bool Organism::applyAnalgesia(std::string id,EffectTarget<injury::Part> target,double seconds,double amount) {
    ActiveEffect effect{std::move(id),std::string(analgesiaEffectType),std::move(target),seconds,{{"relief",amount}}};
    return findEffect(effect.id) ? updateEffect(std::move(effect)) : addEffect(std::move(effect));
}

double Organism::analgesiaRemainingSeconds(injury::Part part) const {
    if (!injuries.anatomy().find(part)) return 0;
    double remaining = 0;
    for (const auto& effect : effects_) {
        if (effect.typeId == analgesiaEffectType && effect.affects(part))
            remaining = std::max(remaining,effect.remainingSeconds.value_or(0));
    }
    return remaining;
}

void Organism::advanceEffects(double seconds, const EffectTick& onTick) {
    if (!std::isfinite(seconds) || seconds <= 0 || effects_.empty()) return;
    // The snapshot lets a handler add, remove or change effects for the next step.
    const auto snapshot = onTick ? effects_ : std::vector<ActiveEffect>{};
    const double tolerance = std::min(1e-9,seconds * 1e-9);
    for (auto& effect : effects_) {
        if (!effect.remainingSeconds) continue;
        const double remaining = *effect.remainingSeconds - seconds;
        effect.remainingSeconds = remaining > tolerance ? remaining : 0;
    }
    effects_.erase(std::remove_if(effects_.begin(),effects_.end(),[](const auto& effect) {
        return effect.remainingSeconds && *effect.remainingSeconds == 0;
    }),effects_.end());
    for (const auto& effect : snapshot)
        onTick(effect,effect.remainingSeconds ? std::min(seconds,*effect.remainingSeconds) : seconds);
}

double Organism::nextEffectEventSeconds() const {
    double next = std::numeric_limits<double>::infinity();
    for (const auto& effect : effects_)
        if (effect.remainingSeconds) next = std::min(next,*effect.remainingSeconds);
    return next;
}

void Organism::advanceCondition(double seconds,const EffectTick& onTick) {
    injuries.advanceBlood(seconds);
    advanceEffects(seconds,onTick);
}

double Organism::nextConditionEventSeconds() const {
    return std::min(injuries.nextBloodEventSeconds(),nextEffectEventSeconds());
}

double Organism::injuryMultiplier(bool injury::ProfilePart::*affected) const {
    double result = 1;
    for (const auto& wound : injuries.wounds()) {
        if (!(injuries.profile().part(wound.part)->*affected)) continue;
        double strongest = 0;
        for (const auto& effect : effects_)
            if (effect.typeId == analgesiaEffectType && effect.affects(wound.part))
                strongest = std::max(strongest,relief(effect));
        const double baseline = wound.impairmentMultiplier();
        result = std::min(result,baseline + (1-baseline)*strongest);
    }
    return result;
}

double Organism::movementMultiplier() const {
    if (effects_.empty()) return injuries.movementMultiplier();
    return injuries.canWalk() ? injuryMultiplier(&injury::ProfilePart::affectsMovement) : 0;
}

double Organism::armMultiplier() const {
    return effects_.empty() ? injuries.armMultiplier() : injuryMultiplier(&injury::ProfilePart::affectsArmFunction);
}

} // namespace colony::physiology
