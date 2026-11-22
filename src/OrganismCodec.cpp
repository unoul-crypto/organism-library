#include "physiology/OrganismCodec.hpp"
#include "physiology/OrganismJson.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace colony::organism_codec {
namespace {
void require(bool valid, const char* message) {
    if (!valid) throw std::invalid_argument(message);
}

int intValue(const Json& value) {
    require(value.is_number_integer(),"Expected an integer.");
    if (value.is_number_unsigned())
        require(value.get<std::uint64_t>() <= std::numeric_limits<int>::max(),"Integer is out of range.");
    else {
        const auto number = value.get<std::int64_t>();
        require(number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max(),
            "Integer is out of range.");
    }
    return value.get<int>();
}

double number(const Json& value) {
    require(value.is_number(),"Expected a number.");
    const auto result = value.get<double>();
    require(std::isfinite(result),"Expected a finite number.");
    return result;
}

bool flag(const Json& value) {
    const int result = intValue(value);
    require(result == 0 || result == 1,"Expected a 0/1 flag.");
    return result != 0;
}

void encodeNutrition(Json& value, const physiology::Nutrition& nutrition) {
    value["hunger"] = nutrition.hunger.level;
    value["starvation"] = nutrition.starvation.level;
    value["hungerUrgent"] = nutrition.hungry() ? 1 : 0;
    value["hungerEmergency"] = nutrition.emergency ? 1 : 0;
}

physiology::Nutrition decodeNutrition(const Json& value) {
    physiology::Nutrition nutrition;
    const double hunger = number(value.at("hunger"));
    const double starvation = number(value.at("starvation"));
    require(hunger >= 0 && hunger <= NeedMeter::kMaximum &&
        starvation >= 0 && starvation <= NeedMeter::kMaximum,"Invalid nutrition level.");
    nutrition.hunger.level = hunger;
    nutrition.starvation.level = starvation;
    nutrition.hunger.urgent = flag(value.at("hungerUrgent"));
    nutrition.emergency = flag(value.at("hungerEmergency"));
    nutrition.hunger.refreshUrgency();
    nutrition.hunger.refreshEmergency(nutrition.emergency);
    return nutrition;
}

Json encodeProfile(const injury::ProfileDefinition& profile) {
    auto parts = Json::array();
    for (const auto& part : profile.parts) parts.push_back({
        {"id",static_cast<int>(part.id)},{"name",part.name},{"hitUpperBound",part.hitUpperBound},
        {"affectsMovement",part.affectsMovement ? 1 : 0},{"affectsArmFunction",part.affectsArmFunction ? 1 : 0}});
    const auto& blood = profile.blood;
    const auto& trauma = profile.trauma;
    const auto& recovery = profile.recovery;
    return Json{{"id",profile.id},{"parts",std::move(parts)},
        {"blood",{{"capacityMl",blood.capacityMl},{"incapacitationFraction",blood.incapacitationFraction},
            {"regenerationFractionPerSecond",blood.regenerationFractionPerSecond},{"lossPerOpenDamage",blood.lossPerOpenDamage}}},
        {"trauma",{{"incapacitating",trauma.incapacitating},{"lethal",trauma.lethal},{"injuryChance",trauma.injuryChance},
            {"mildWoundDamage",trauma.mildWoundDamage},{"seriousWoundDamage",trauma.seriousWoundDamage},
            {"criticalWoundDamage",trauma.criticalWoundDamage},
            {"movementTrauma",trauma.movementTrauma ? Json(*trauma.movementTrauma) : Json(nullptr)},
            {"movementWoundSeverity",trauma.movementWoundSeverity ?
                Json(static_cast<int>(*trauma.movementWoundSeverity)) : Json(nullptr)}}},
        {"recovery",{{"bedRestSeconds",recovery.bedRestSeconds},{"untreatedRate",recovery.untreatedRate},
            {"treatedRate",recovery.treatedRate}}}};
}

injury::Profile decodeProfile(const Json& json) {
    injury::ProfileDefinition profile;
    profile.id = json.at("id").get<std::string>();
    const auto& parts = json.at("parts");
    require(parts.is_array(),"Expected an array of body parts.");
    for (const auto& part : parts)
        profile.parts.push_back({static_cast<injury::Part>(intValue(part.at("id"))),part.at("name").get<std::string>(),
            number(part.at("hitUpperBound")),flag(part.at("affectsMovement")),flag(part.at("affectsArmFunction"))});
    const auto& blood = json.at("blood");
    profile.blood = {number(blood.at("capacityMl")),number(blood.at("incapacitationFraction")),
        number(blood.at("regenerationFractionPerSecond")),number(blood.at("lossPerOpenDamage"))};
    const auto& trauma = json.at("trauma");
    profile.trauma.incapacitating = intValue(trauma.at("incapacitating"));
    profile.trauma.lethal = intValue(trauma.at("lethal"));
    profile.trauma.injuryChance = number(trauma.at("injuryChance"));
    profile.trauma.mildWoundDamage = number(trauma.at("mildWoundDamage"));
    profile.trauma.seriousWoundDamage = number(trauma.at("seriousWoundDamage"));
    profile.trauma.criticalWoundDamage = number(trauma.at("criticalWoundDamage"));
    const auto& movementTrauma = trauma.at("movementTrauma");
    if (!movementTrauma.is_null()) profile.trauma.movementTrauma = intValue(movementTrauma);
    const auto& movementSeverity = trauma.at("movementWoundSeverity");
    if (!movementSeverity.is_null())
        profile.trauma.movementWoundSeverity = static_cast<injury::Severity>(intValue(movementSeverity));
    const auto& recovery = json.at("recovery");
    const auto& durations = recovery.at("bedRestSeconds");
    require(durations.is_array() && durations.size() == profile.recovery.bedRestSeconds.size(),
        "Expected one rest duration per wound severity.");
    for (std::size_t i = 0; i < durations.size(); ++i) profile.recovery.bedRestSeconds[i] = number(durations[i]);
    profile.recovery.untreatedRate = number(recovery.at("untreatedRate"));
    profile.recovery.treatedRate = number(recovery.at("treatedRate"));
    return injury::Profile(std::move(profile));
}

void restoreInjuries(injury::State& result, const Json& value, bool allowMissingOpenDamage) {
    require(value.is_array(),"Expected an array of injuries.");
    for (const auto& wound : value) {
        const auto part = static_cast<injury::Part>(intValue(wound.at("part")));
        const auto severity = static_cast<injury::Severity>(intValue(wound.at("severity")));
        const double openDamage = allowMissingOpenDamage && !wound.contains("openDamage") ?
            0 : number(wound.at("openDamage"));
        require(result.restoreWound({part,severity,number(wound.at("restRemaining")),flag(wound.at("treated")),
            number(wound.at("damage")),openDamage}),"Invalid injury.");
    }
}

void writeStateFields(Json& value, const physiology::Organism& organism, bool includeNutrition) {
    value["injuries"] = encodeInjuries(organism.injuries);
    value["bloodVolumeMl"] = organism.injuries.bloodVolume();
    if (includeNutrition) encodeNutrition(value,organism.nutrition);
}

physiology::Organism readStateFields(const Json& value, physiology::Organism result, bool includeNutrition) {
    if (value.contains("organismProfile"))
        require(result.injuries.body() == injury::Body::Custom &&
            value.at("organismProfile") == result.injuries.profile().definition().id,"Wrong organism profile.");
    restoreInjuries(result.injuries,value.at("injuries"),false);
    require(result.injuries.restoreBloodVolume(number(value.at("bloodVolumeMl"))),"Invalid blood volume.");
    if (includeNutrition) result.nutrition = decodeNutrition(value);
    return result;
}

physiology::Organism decodeLegacy(const Json& value, bool includeNutrition) {
    const auto body = static_cast<injury::Body>(intValue(value.at("body")));
    const auto resilience = static_cast<injury::Resilience>(intValue(value.at("resilience")));
    require(body==injury::Body::Humanoid || body==injury::Body::Quadruped,"Invalid body type.");
    require(resilience==injury::Resilience::Normal || resilience==injury::Resilience::Robust,"Invalid resilience.");
    return decodeFields(value,body,resilience,includeNutrition);
}
}

Json encodeInjuries(const injury::State& injuries) {
    auto result = Json::array();
    for (const auto& wound : injuries.wounds())
        result.push_back(Json{{"part",static_cast<int>(wound.part)},{"severity",static_cast<int>(wound.severity)},
            {"restRemaining",wound.restRemaining},{"treated",wound.treated?1:0},{"damage",wound.damage},
            {"openDamage",wound.openDamage}});
    return result;
}

injury::State decodeInjuries(const Json& value, injury::Body body, injury::Resilience resilience,
                            bool allowMissingOpenDamage) {
    injury::State result{body,resilience};
    restoreInjuries(result,value,allowMissingOpenDamage);
    return result;
}

void encodeFields(Json& value, const physiology::Organism& organism, bool includeNutrition) {
    writeStateFields(value,organism,includeNutrition);
    if (organism.injuries.body() == injury::Body::Custom)
        value["organismProfile"] = organism.injuries.profile().definition().id;
    else value.erase("organismProfile");
}

physiology::Organism decodeFields(const Json& value, injury::Body body, injury::Resilience resilience,
                                 bool includeNutrition) {
    return readStateFields(value,physiology::Organism{body,resilience},includeNutrition);
}

physiology::Organism decodeFields(const Json& value, const injury::Profile& profile, bool includeNutrition) {
    return readStateFields(value,physiology::Organism{profile},includeNutrition);
}

std::string encode(const physiology::Organism& organism) {
    const auto& injuries = organism.injuries;
    Json value;
    if (injuries.body() == injury::Body::Custom)
        value = Json{{"formatVersion",3},{"profile",encodeProfile(injuries.profile().definition())}};
    else value = Json{{"formatVersion",2},{"body",static_cast<int>(injuries.body())},
        {"resilience",static_cast<int>(injuries.resilience())}};
    writeStateFields(value,organism,true);
    return value.dump();
}

std::optional<physiology::Organism> decode(std::string_view json, std::string& error) {
    try {
        const auto value = Json::parse(json);
        const int version = intValue(value.at("formatVersion"));
        require(version >= 1 && version <= 3,"Unsupported organism format.");
        auto result = version == 3 ? decodeFields(value,decodeProfile(value.at("profile")),true) :
            decodeLegacy(value,version == 2);
        error.clear();
        return result;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}
} // namespace colony::organism_codec
