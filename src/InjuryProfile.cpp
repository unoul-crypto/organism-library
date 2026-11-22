#include "physiology/InjuryProfile.hpp"
#include "physiology/BloodVolume.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace colony::injury {
namespace {

bool positive(double value) {
    return std::isfinite(value) && value > 0;
}

std::vector<physiology::PartDefinition<Part>> anatomyParts(const ProfileDefinition& definition) {
    if (definition.id.empty() || definition.parts.empty())
        throw std::invalid_argument("An injury profile requires an ID and body parts.");

    std::unordered_set<int> ids;
    std::vector<physiology::PartDefinition<Part>> parts;
    parts.reserve(definition.parts.size());
    double previous = 0;
    for (const auto& part : definition.parts) {
        const int id = static_cast<int>(part.id);
        if (id < 0 || part.name.empty() || !ids.insert(id).second ||
            !std::isfinite(part.hitUpperBound) || part.hitUpperBound <= previous || part.hitUpperBound > 1)
            throw std::invalid_argument("Invalid body part or hit range.");
        parts.push_back({part.id,part.name,part.hitUpperBound});
        previous = part.hitUpperBound;
    }
    if (previous != 1)
        throw std::invalid_argument("The last body part must end the hit range at 1.");
    return parts;
}

void validateParameters(const ProfileDefinition& definition) {
    const auto& blood = definition.blood;
    physiology::BloodVolume{blood.capacityMl,blood.incapacitationFraction,blood.regenerationFractionPerSecond};
    if (!std::isfinite(blood.lossPerOpenDamage) || blood.lossPerOpenDamage < 0 ||
        blood.lossPerOpenDamage > std::numeric_limits<double>::max() / maximumDamage)
        throw std::invalid_argument("Invalid blood loss rate.");

    const auto& trauma = definition.trauma;
    if (trauma.incapacitating <= 0 || trauma.lethal < trauma.incapacitating ||
        !positive(trauma.mildWoundDamage) || !positive(trauma.seriousWoundDamage) ||
        !positive(trauma.criticalWoundDamage) || trauma.mildWoundDamage >= trauma.seriousWoundDamage ||
        trauma.seriousWoundDamage >= trauma.criticalWoundDamage || trauma.criticalWoundDamage > maximumDamage ||
        !std::isfinite(trauma.injuryChance) || trauma.injuryChance < 0 || trauma.injuryChance > 1 ||
        (trauma.movementTrauma && *trauma.movementTrauma <= 0) ||
        (trauma.movementWoundSeverity && (*trauma.movementWoundSeverity < Severity::Mild ||
            *trauma.movementWoundSeverity > Severity::Critical)))
        throw std::invalid_argument("Invalid injury thresholds.");

    const auto& recovery = definition.recovery;
    if (!positive(recovery.untreatedRate) || !positive(recovery.treatedRate))
        throw std::invalid_argument("Invalid wound recovery rate.");
    for (double duration : recovery.bedRestSeconds) {
        if (!positive(duration) || !std::isfinite(duration / std::min(recovery.untreatedRate,recovery.treatedRate)))
            throw std::invalid_argument("Invalid wound recovery duration.");
    }
}

} // namespace

struct Profile::Data {
    explicit Data(ProfileDefinition value)
        : definition(std::move(value)), parts(anatomyParts(definition)), anatomy(parts) {
        validateParameters(definition);
    }

    ProfileDefinition definition;
    std::vector<physiology::PartDefinition<Part>> parts;
    physiology::Anatomy<Part> anatomy;
};

Profile::Profile(ProfileDefinition definition) : data_(std::make_shared<const Data>(std::move(definition))) {}

const ProfileDefinition& Profile::definition() const { return data_->definition; }

const physiology::Anatomy<Part>& Profile::anatomy() const { return data_->anatomy; }

const ProfilePart* Profile::part(Part id) const {
    const auto& parts = definition().parts;
    const auto entry = std::find_if(parts.begin(),parts.end(),[id](const auto& part) { return part.id == id; });
    return entry == parts.end() ? nullptr : &*entry;
}

} // namespace colony::injury
