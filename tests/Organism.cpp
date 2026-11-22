#include "physiology/OrganismCodec.hpp"
#include "physiology/OrganismJson.hpp"

#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace colony;
using namespace colony::injury;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void nutritionForecast() {
    physiology::Organism organism;
    auto& nutrition = organism.nutrition;
    nutrition.setHungerLevel(79);
    require(nutrition.nextEventSeconds() == 18,"Forecast urgent hunger from its current level");
    nutrition.advance(18);
    require(nutrition.hungry() && nutrition.nextEventSeconds() == 360,"Forecast the onset of starvation next");
    nutrition.advance(360);
    require(nutrition.emergency && nutrition.nextEventSeconds() == Starvation::kDeathSeconds,
        "Forecast death once starvation begins");
    nutrition.advance(60);
    nutrition.eat(55);
    require(!nutrition.hungry() && std::abs(nutrition.nextEventSeconds()-30)<1e-8,
        "Food releases the hunger signal and sets a recovery deadline");
    nutrition.advance(30);
    require(organism.alive() && nutrition.starvation.level==0,"Recovery clears starvation");
}

void anatomyAndWounds() {
    enum class PartId { Core, Tail, Sail };
    constexpr std::array parts{
        physiology::PartDefinition<PartId>{PartId::Core,"Core",.8},
        physiology::PartDefinition<PartId>{PartId::Tail,"Tail",1}
    };
    const physiology::Anatomy<PartId> anatomy{parts};
    using Target = physiology::EffectTarget<PartId>;
    require(anatomy.hitPart(.8)==PartId::Tail && anatomy.supports(Target{physiology::WholeBody{}}) &&
        anatomy.supports(Target{std::vector{PartId::Core,PartId::Tail}}),"Support a custom anatomy and systemic or multi-part targets");
    require(!anatomy.supports(Target{PartId::Sail}) &&
        !anatomy.supports(Target{std::vector{PartId::Tail,PartId::Tail}}) &&
        !anatomy.supports(Target{std::vector<PartId>{}}),"Reject missing or duplicate targeted parts");

    State patient;
    require(patient.applyDamage(Part::Arm,4,WoundKind::Open) && patient.applyDamage(Part::Torso,6,WoundKind::Open) &&
        patient.applyDamage(Part::Head,3),"Create two open wounds and an impact wound");
    require(patient.bloodLossRate()==5,"Only open damage contributes to bleeding");
    patient.advanceBlood(4);
    require(patient.bloodVolume()==4980,"Subtract the sum of the two rates exactly once");
    patient.treat();
    require(patient.bloodLossRate()==2,"Treat only the worst wound");
    patient.advanceBlood(1);
    require(patient.bloodVolume()==4978 && patient.applyDamage(Part::Torso,2) &&
        patient.bloodLossRate()==2,"A blunt impact does not reopen previously bandaged damage");
    require(patient.applyDamage(Part::Arm,2,WoundKind::Open) && patient.bloodLossRate()==3,
        "A new cut adds to the active bleeding damage on its part");
    patient.treat();
    patient.treat();
    require(patient.bloodLossRate()==0,"Bandaging all open wounds stops the total loss");
    patient.advanceBlood(10);
    require(patient.bloodVolume()==5000,"Recover blood up to capacity after bleeding stops");
}

void thresholdsAndJson() {
    physiology::Organism organism{Body::Quadruped,Resilience::Robust};
    auto& injuries = organism.injuries;
    require(injuries.applyDamage(Part::Torso,16,WoundKind::Open) &&
        injuries.applyDamage(Part::Head,4,WoundKind::Open) && injuries.restoreBloodVolume(2760),
        "Seed a wounded boar just above the threshold");
    require(injuries.nextBloodEventSeconds()==1 && injuries.canWalk(),"Forecast arrival at half capacity");
    injuries.advanceBlood(1.25);
    require(injuries.downed(),"Severe blood loss incapacitates independently of trauma");
    injuries.treat();
    organism.nutrition.setHungerLevel(100);
    organism.nutrition.advance(75);
    organism.nutrition.eat(42);
    std::string error;
    const auto json = organism_codec::encode(organism);
    auto copy = organism_codec::decode(json,error);
    require(copy && error.empty() && organism_codec::encode(*copy)==json &&
        copy->injuries.maximumBloodVolume()==5500 && copy->injuries.bloodLossRate()==2 && copy->injuries.downed() &&
        copy->nutrition.hunger.level==58 && copy->nutrition.hungry() && copy->nutrition.emergency &&
        copy->nutrition.starvation.level==12.5,
        "Round-trip profile, wounds, treatment, hunger hysteresis and starvation in standalone JSON");
    copy->injuries.treat();
    copy->injuries.advanceBlood(copy->injuries.nextBloodEventSeconds());
    require(copy->injuries.canWalk() && copy->injuries.bloodVolume()==2750,"Forecast when regeneration restores mobility");
    auto legacy = organism_codec::Json::parse(json);
    legacy["formatVersion"] = 1;
    for (const auto* field : {"hunger","hungerUrgent","hungerEmergency","starvation"}) legacy.erase(field);
    copy = organism_codec::decode(legacy.dump(),error);
    require(copy && copy->injuries.bloodVolume()==injuries.bloodVolume() && copy->nutrition.hunger.level==0 &&
        copy->nutrition.starvation.level==0 && !copy->nutrition.hungry(),"Legacy organism JSON starts with default nutrition");
    injuries.advanceBlood(5000);
    require(injuries.bloodVolume()==0 && !organism.alive(),"Clamp fatal bleeding at zero");
    injuries.advanceBlood(1);
    require(injuries.bloodVolume()==0 && std::isinf(injuries.nextBloodEventSeconds()),"A dead organism stays dead");
    copy = organism_codec::decode(organism_codec::encode(organism),error);
    require(copy && !copy->alive(),"A fatal blood loss round-trips in the standalone organism record");
    auto invalid = organism_codec::Json::parse(json);
    invalid["bloodVolumeMl"] = 5501;
    require(!organism_codec::decode(invalid.dump(),error) && !error.empty(),"Reject blood outside profile capacity");
    invalid = organism_codec::Json::parse(json);
    invalid["injuries"][0]["openDamage"] = 17;
    require(!organism_codec::decode(invalid.dump(),error),"Reject bleeding damage in excess of wound damage");
    invalid = organism_codec::Json::parse(json);
    invalid.erase("bloodVolumeMl");
    require(!organism_codec::decode(invalid.dump(),error),"Reject a missing volume in a current organism export");
    invalid = organism_codec::Json::parse(json);
    invalid["starvation"] = 101;
    require(!organism_codec::decode(invalid.dump(),error),"Reject starvation outside the nutrition range");
}

} // namespace

int main() {
    try {
        anatomyAndWounds();
        nutritionForecast();
        thresholdsAndJson();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
