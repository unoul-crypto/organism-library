#include "physiology/OrganismCodec.hpp"
#include "physiology/OrganismJson.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

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
    require(value.is_array(),"Expected an array of injuries.");
    injury::State result{body,resilience};
    for (const auto& wound : value) {
        const auto part = static_cast<injury::Part>(intValue(wound.at("part")));
        const auto severity = static_cast<injury::Severity>(intValue(wound.at("severity")));
        const double openDamage = allowMissingOpenDamage && !wound.contains("openDamage") ?
            0 : number(wound.at("openDamage"));
        require(result.restoreWound({part,severity,number(wound.at("restRemaining")),flag(wound.at("treated")),
            number(wound.at("damage")),openDamage}),"Invalid injury.");
    }
    return result;
}

void encodeFields(Json& value, const physiology::Organism& organism, bool includeNutrition) {
    value["injuries"] = encodeInjuries(organism.injuries);
    value["bloodVolumeMl"] = organism.injuries.bloodVolume();
    if (includeNutrition) encodeNutrition(value,organism.nutrition);
}

physiology::Organism decodeFields(const Json& value, injury::Body body, injury::Resilience resilience,
                                 bool includeNutrition) {
    physiology::Organism result;
    result.injuries = decodeInjuries(value.at("injuries"),body,resilience);
    require(result.injuries.restoreBloodVolume(number(value.at("bloodVolumeMl"))),"Invalid blood volume.");
    if (includeNutrition) result.nutrition = decodeNutrition(value);
    return result;
}
std::string encode(const physiology::Organism& organism) {
    Json value{{"formatVersion",2},{"body",static_cast<int>(organism.injuries.body())},
        {"resilience",static_cast<int>(organism.injuries.resilience())}};
    encodeFields(value,organism,true);
    return value.dump();
}

std::optional<physiology::Organism> decode(std::string_view json, std::string& error) {
    try {
        const auto value = Json::parse(json);
        const int version = intValue(value.at("formatVersion"));
        require(version == 1 || version == 2,"Unsupported organism format.");
        const auto body = static_cast<injury::Body>(intValue(value.at("body")));
        const auto resilience = static_cast<injury::Resilience>(intValue(value.at("resilience")));
        require(body==injury::Body::Humanoid || body==injury::Body::Quadruped,"Invalid body type.");
        require(resilience==injury::Resilience::Normal || resilience==injury::Resilience::Robust,"Invalid resilience.");
        auto result = decodeFields(value,body,resilience,version == 2);
        error.clear();
        return result;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}
} // namespace colony::organism_codec
