#include "physiology/Injuries.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <iterator>

namespace colony::injury {
namespace {
constexpr std::array humanoidParts{
    physiology::PartDefinition<Part>{Part::Leg,"Leg",.25}, physiology::PartDefinition<Part>{Part::Arm,"Arm",.45},
    physiology::PartDefinition<Part>{Part::Torso,"Torso",.90}, physiology::PartDefinition<Part>{Part::Head,"Head",1}
};
constexpr std::array quadrupedParts{
    physiology::PartDefinition<Part>{Part::Leg,"Leg",.30}, physiology::PartDefinition<Part>{Part::Torso,"Torso",.90},
    physiology::PartDefinition<Part>{Part::Head,"Head",1}
};
constexpr physiology::Anatomy<Part> humanoidAnatomy{humanoidParts};
constexpr physiology::Anatomy<Part> quadrupedAnatomy{quadrupedParts};

double bloodCapacity(Body body, Resilience resilience) {
    if (body == Body::Humanoid) return 5000;
    return resilience == Resilience::Robust ? 5500 : 4000;
}
}

State::State(Body body, Resilience resilience)
    : body_(body), resilience_(resilience), blood_(bloodCapacity(body,resilience),.5,.001) {}

const physiology::Anatomy<Part>& State::anatomy() const {
    return body_ == Body::Humanoid ? humanoidAnatomy : quadrupedAnatomy;
}

double State::bloodLossRate() const {
    if (!alive()) return 0;
    double rate = 0;
    for (const auto& wound : wounds_) rate += wound.bloodLossRate();
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
    switch (severity) {
    case Severity::Mild: return 4;
    case Severity::Serious: return seriousDamage;
    case Severity::Critical: return criticalDamage;
    }
    return 0;
}

Severity damageSeverity(double damage) {
    return damage>=criticalDamage ? Severity::Critical : damage>=seriousDamage ? Severity::Serious : Severity::Mild;
}

void State::refreshTrauma() {
    trauma_=0;
    for (const auto& wound:wounds_) trauma_+=static_cast<int>(wound.damage/seriousDamage);
}

double bedRestDuration(Severity severity) {
    switch (severity) {
    case Severity::Mild: return 30;
    case Severity::Serious: return 90;
    case Severity::Critical: return 180;
    }
    return 0;
}

bool State::canWalk() const {
    if (!alive() || blood_.incapacitated() || trauma_ >= incapacitationThreshold()) return false;
    if (resilience_==Resilience::Robust) {
        int legTrauma=0;
        for (const auto& wound:wounds_) if (wound.part==Part::Leg) legTrauma+=static_cast<int>(wound.damage/seriousDamage);
        return legTrauma<robustLegIncapacitatingTrauma;
    }
    const auto legLimit=body_==Body::Humanoid ? Severity::Critical : Severity::Serious;
    return std::none_of(wounds_.begin(),wounds_.end(),[&](const Wound& wound) {
        return wound.part==Part::Leg && wound.severity>=legLimit;
    });
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
    const auto severity=damageSeverity(damage);
    const Wound replacement{part,severity,bedRestDuration(severity),false,damage,std::min(openDamage,damage)};
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
    return record(part,severity,bedRestDuration(severity));
}

bool State::record(Part part, Severity severity, double restRemaining, bool treated, double damage) {
    if (damage==0) damage=severityDamage(severity);
    return alive() && restoreWound({part,severity,restRemaining,treated,damage});
}

bool State::restoreWound(const Wound& wound) {
    const auto part=wound.part;
    const auto severity=wound.severity;
    const double damage=wound.damage, restRemaining=wound.restRemaining;
    if (!anatomy().supports(wound.target()) || severity<Severity::Mild || severity>Severity::Critical ||
        !std::isfinite(restRemaining) ||
        restRemaining<=0 || restRemaining>bedRestDuration(severity) || !std::isfinite(damage) || damage<=0 ||
        damage>maximumDamage || damageSeverity(damage)!=severity ||
        !std::isfinite(wound.openDamage) || wound.openDamage<0 || wound.openDamage>damage) return false;
    if (severity==Severity::Mild && std::any_of(wounds_.begin(),wounds_.end(),[&](const Wound& wound) {
        return wound.part==part && wound.severity==Severity::Mild;
    })) return false;
    wounds_.push_back(wound);
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

double State::multiplier(Part part) const {
    constexpr std::array speed{0.90,0.75,0.60};
    double result = 1.0;
    for (const auto& wound : wounds_)
        if (wound.part==part) result = std::min(result,speed[static_cast<std::size_t>(wound.severity)]);
    return result;
}

} // namespace colony::injury
