#include "physiology/Injuries.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <iterator>
#include <utility>

namespace colony::injury {
namespace {
Profile makeBuiltinProfile(bool humanoid, bool robust) {
    ProfileDefinition definition;
    if (humanoid) {
        definition.id = robust ? "robust-humanoid" : "humanoid";
        definition.parts = {{Part::Leg,"Leg",.25,true},{Part::Arm,"Arm",.45,false,true},
            {Part::Torso,"Torso",.90},{Part::Head,"Head",1}};
    } else {
        definition.id = robust ? "robust-quadruped" : "quadruped";
        definition.parts = {{Part::Leg,"Leg",.30,true},{Part::Torso,"Torso",.90},{Part::Head,"Head",1}};
        definition.blood.capacityMl = robust ? 5500 : 4000;
        definition.trauma.injuryChance = .85;
    }

    auto& trauma = definition.trauma;
    if (robust) {
        trauma.incapacitating = State::robustIncapacitatingTrauma;
        trauma.lethal = State::robustLethalTrauma;
        trauma.movementTrauma = State::robustLegIncapacitatingTrauma;
    } else {
        trauma.movementWoundSeverity = humanoid ? Severity::Critical : Severity::Serious;
        if (!humanoid) {
            trauma.incapacitating = State::quadrupedIncapacitatingTrauma;
            trauma.lethal = State::quadrupedLethalTrauma;
        }
    }
    return Profile(std::move(definition));
}

const Profile& builtinProfile(Body body, Resilience resilience) {
    static const Profile humanoid = makeBuiltinProfile(true,false);
    static const Profile robustHumanoid = makeBuiltinProfile(true,true);
    static const Profile quadruped = makeBuiltinProfile(false,false);
    static const Profile robustQuadruped = makeBuiltinProfile(false,true);
    if (resilience != Resilience::Normal && resilience != Resilience::Robust)
        throw std::invalid_argument("Invalid resilience.");
    if (body == Body::Humanoid) return resilience == Resilience::Robust ? robustHumanoid : humanoid;
    if (body == Body::Quadruped) return resilience == Resilience::Robust ? robustQuadruped : quadruped;
    throw std::invalid_argument("A custom body requires a profile.");
}

constexpr auto severityIndex(Severity severity) { return static_cast<std::size_t>(severity); }

Severity classify(double damage, const TraumaParameters& traits) {
    if (damage >= traits.criticalWoundDamage) return Severity::Critical;
    if (damage >= traits.seriousWoundDamage) return Severity::Serious;
    return Severity::Mild;
}

double representativeDamage(Severity severity, const TraumaParameters& traits) {
    switch (severity) {
    case Severity::Mild: return traits.mildWoundDamage;
    case Severity::Serious: return traits.seriousWoundDamage;
    case Severity::Critical: return traits.criticalWoundDamage;
    }
    return 0;
}

int addTrauma(int first, int second) {
    return first + std::min(second,std::numeric_limits<int>::max()-first);
}
}

State::State(Body body, Resilience resilience) : State(builtinProfile(body,resilience)) {
    body_ = body;
    resilience_ = resilience;
}

State::State(Profile profile)
    : profile_(std::move(profile)), blood_(profile_.definition().blood.capacityMl,
        profile_.definition().blood.incapacitationFraction,profile_.definition().blood.regenerationFractionPerSecond) {}

Wound State::configuredWound(Wound wound) const {
    const auto& rules = profile_.definition();
    wound.untreatedRecoveryRate = rules.recovery.untreatedRate;
    wound.treatedRecoveryRate = rules.recovery.treatedRate;
    wound.lossPerOpenDamage = rules.blood.lossPerOpenDamage;
    return wound;
}

double State::bloodLossRate() const {
    if (!alive()) return 0;
    double rate = 0;
    constexpr double maximumRate = std::numeric_limits<double>::max();
    for (const auto& wound : wounds_) rate += std::min(wound.bloodLossRate(),maximumRate-rate);
    return rate;
}

double State::nextBloodEventSeconds() const {
    return alive() ? blood_.nextEventSeconds(bloodLossRate()) : std::numeric_limits<double>::infinity();
}

void State::advanceBlood(double seconds) {
    if (alive() && (!wounds_.empty() || bloodVolume() < maximumBloodVolume()))
        blood_.advance(seconds,bloodLossRate());
}

double severityDamage(Severity severity) {
    return representativeDamage(severity,TraumaParameters{});
}

Severity damageSeverity(double damage) {
    return classify(damage,TraumaParameters{});
}

int State::traumaFromDamage(double damage) const {
    return static_cast<int>(std::min(damage / profile_.definition().trauma.seriousWoundDamage,
        static_cast<double>(std::numeric_limits<int>::max())));
}

void State::refreshTrauma() {
    trauma_=0;
    for (const auto& wound:wounds_) trauma_=addTrauma(trauma_,traumaFromDamage(wound.damage));
}

double bedRestDuration(Severity severity) {
    const auto durations = RecoveryParameters{}.bedRestSeconds;
    const auto index = severityIndex(severity);
    return index < durations.size() ? durations[index] : 0;
}

bool State::canWalk() const {
    if (!alive() || blood_.incapacitated() || trauma_ >= incapacitationThreshold()) return false;
    const auto& traits = profile_.definition().trauma;
    if (!traits.movementTrauma && !traits.movementWoundSeverity) return true;

    int movementTrauma = 0;
    for (const auto& wound : wounds_) {
        if (!profile_.part(wound.part)->affectsMovement) continue;
        if (traits.movementWoundSeverity && wound.severity >= *traits.movementWoundSeverity) return false;
        if (traits.movementTrauma) {
            movementTrauma = addTrauma(movementTrauma,traumaFromDamage(wound.damage));
            if (movementTrauma >= *traits.movementTrauma) return false;
        }
    }
    return true;
}

double State::movementMultiplier() const {
    return canWalk() ? multiplier(&ProfilePart::affectsMovement) : 0;
}

double State::armMultiplier() const {
    return multiplier(&ProfilePart::affectsArmFunction);
}

Part State::hitPart(double roll) const {
    return anatomy().hitPart(roll);
}

bool State::applyDamage(Part part,double damage,WoundKind kind) {
    if (!alive() || !anatomy().find(part) ||
        !std::isfinite(damage) || damage<=0 || damage>maximumDamage) return false;
    // Old saves may contain several wounds on a part; a fresh hit combines their damage.
    double openDamage = kind == WoundKind::Open ? damage : 0;
    for (const auto& wound:wounds_) if (wound.part==part) {
        damage += wound.damage;
        if (!wound.treated) openDamage += wound.openDamage;
    }
    damage=std::min(damage,maximumDamage);
    const auto& rules = profile_.definition();
    const auto severity = classify(damage,rules.trauma);
    const Wound replacement = configuredWound({part,severity,rules.recovery.bedRestSeconds[severityIndex(severity)],
        false,damage,std::min(openDamage,damage)});
    auto existing=std::find_if(wounds_.begin(),wounds_.end(),[&](const Wound& wound) { return wound.part==part; });
    if (existing==wounds_.end()) wounds_.push_back(replacement);
    else {
        *existing=replacement;
        wounds_.erase(std::remove_if(std::next(existing),wounds_.end(),[&](const Wound& wound) { return wound.part==part; }),wounds_.end());
    }
    refreshTrauma();
    return true;
}

bool State::record(Part part, Severity severity) {
    const auto index = severityIndex(severity);
    const auto& durations = profile_.definition().recovery.bedRestSeconds;
    return index < durations.size() && record(part,severity,durations[index]);
}

bool State::record(Part part, Severity severity, double restRemaining, bool treated, double damage) {
    if (damage==0) damage=representativeDamage(severity,profile_.definition().trauma);
    return alive() && restoreWound({part,severity,restRemaining,treated,damage});
}

bool State::restoreWound(const Wound& wound) {
    const auto part=wound.part;
    const auto severity=wound.severity;
    const double damage=wound.damage, restRemaining=wound.restRemaining;
    const auto& rules = profile_.definition();
    const auto index = severityIndex(severity);
    if (!anatomy().find(part) || index >= rules.recovery.bedRestSeconds.size() ||
        !std::isfinite(restRemaining) || restRemaining<=0 || restRemaining>rules.recovery.bedRestSeconds[index] ||
        !std::isfinite(damage) || damage<=0 || damage>maximumDamage || classify(damage,rules.trauma)!=severity ||
        !std::isfinite(wound.openDamage) || wound.openDamage<0 || wound.openDamage>damage) return false;
    if (severity==Severity::Mild && std::any_of(wounds_.begin(),wounds_.end(),[&](const Wound& wound) {
        return wound.part==part && wound.severity==Severity::Mild;
    })) return false;
    wounds_.push_back(configuredWound(wound));
    refreshTrauma();
    return true;
}

void State::restInBed(double seconds) {
    if (!alive() || !std::isfinite(seconds) || seconds<=0) return;
    for (auto& wound : wounds_) wound.restRemaining -= seconds*wound.recoveryRate();
    wounds_.erase(std::remove_if(wounds_.begin(),wounds_.end(),[&](const Wound& wound) {
        if (wound.restRemaining>1e-9) return false;
        return true;
    }),wounds_.end());
    refreshTrauma();
}

double State::nextRecoverySeconds() const {
    double next = std::numeric_limits<double>::infinity();
    if (alive()) for (const auto& wound : wounds_) next = std::min(next,wound.recoverySeconds());
    return next;
}

bool State::needsTreatment() const {
    return alive() && std::any_of(wounds_.begin(),wounds_.end(),[](const Wound& wound) { return !wound.treated; });
}

void State::treat() {
    if (!alive()) return;
    Wound* worst=nullptr;
    for (auto& wound : wounds_) {
        if (wound.treated) continue;
        if (!worst || wound.severity>worst->severity ||
            (wound.severity==worst->severity && wound.damage>worst->damage)) worst=&wound;
    }
    if (worst) worst->treated=true;
}

double State::multiplier(bool ProfilePart::*affected) const {
    constexpr std::array speed{0.90,0.75,0.60};
    double result = 1.0;
    for (const auto& wound : wounds_)
        if (profile_.part(wound.part)->*affected) result = std::min(result,speed[severityIndex(wound.severity)]);
    return result;
}

} // namespace colony::injury
