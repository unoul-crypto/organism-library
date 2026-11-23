#include "physiology/OrganismCodec.hpp"
#include "physiology/OrganismJson.hpp"

#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
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
    require(legacy.at("profileId")=="robust-quadruped","Built-in profiles use stable string IDs");
    legacy.erase("profileId");
    legacy.erase("effects");
    legacy["body"] = 1;
    legacy["resilience"] = 1;
    legacy["formatVersion"] = 2;
    copy = organism_codec::decode(legacy.dump(),error);
    require(copy && organism_codec::encode(*copy)==json,"Legacy format 2 restores its exact nutrition and injuries");
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

constexpr auto leftWing = static_cast<Part>(10);
constexpr auto rightWing = static_cast<Part>(11);
constexpr auto hull = static_cast<Part>(12);

ProfileDefinition gliderDefinition() {
    ProfileDefinition definition;
    definition.id = "glider";
    definition.parts = {{leftWing,"Left wing",.25,true,true},{rightWing,"Right wing",.5,true},
        {hull,"Hull",1}};
    definition.blood = {1200,.6,.01,.75};
    definition.trauma = {6,8,4,8,16,.9,2,std::nullopt};
    definition.recovery = {{8,20,40},.25,2};
    return definition;
}

void customProfile() {
    auto definition = gliderDefinition();
    const Profile profile(definition);
    definition.parts.front().name = "Changed";
    physiology::Organism glider{profile};
    auto& state = glider.injuries;
    require(state.anatomy().find(leftWing)->name=="Left wing" && state.anatomy().find(Part::Leg)==nullptr &&
        state.hitPart(.24)==leftWing && state.hitPart(.25)==rightWing && state.hitPart(.5)==hull,
        "A game-defined profile owns its anatomy and provides custom hit locations");
    require(!state.applyDamage(Part::Leg,8) && state.applyDamage(leftWing,8,WoundKind::Open) &&
        state.wounds()[0].severity==Severity::Serious && state.trauma()==1 && state.injuryChance()==.9 &&
        state.movementMultiplier()==.75 && state.armMultiplier()==.75,"Use the configured severity and body part roles");

    require(state.maximumBloodVolume()==1200 && state.bloodLossRate()==6 &&
        state.nextBloodEventSeconds()==80,"Use the configured blood capacity, loss rate and collapse threshold");
    state.advanceBlood(10);
    state.restInBed(2);
    require(state.bloodVolume()==1140 && state.wounds()[0].restRemaining==19.5 &&
        state.nextRecoverySeconds()==78,"Use custom untreated wound recovery");
    state.treat();
    state.advanceBlood(2);
    require(state.bloodVolume()==1164 && state.nextRecoverySeconds()==9.75 &&
        state.wounds()[0].recoverySeconds()==9.75,"Use custom blood regeneration and recovery after treatment");
    require(state.applyDamage(rightWing,8) && state.downed() && state.trauma()==2,
        "Movement trauma combines on the parts chosen by the game");

    physiology::Organism second{profile};
    require(second.injuries.applyDamage(hull,48) && second.injuries.downed() &&
        second.injuries.applyDamage(hull,16) && !second.alive(),"Use configured general trauma thresholds");

    std::string error;
    const auto json = organism_codec::encode(glider);
    const auto result = organism_codec::decode(json,error);
    require(result && error.empty() && organism_codec::encode(*result)==json &&
        result->injuries.downed() && result->injuries.wounds()[0].recoverySeconds()==9.75 &&
        result->injuries.profile().definition().id=="glider","Custom profiles and wounds survive standalone JSON");

    auto legacy = organism_codec::Json::parse(json);
    legacy["formatVersion"] = 3;
    legacy.erase("profileId");
    legacy.erase("effects");
    const auto legacyResult = organism_codec::decode(legacy.dump(),error);
    require(legacyResult && organism_codec::encode(*legacyResult)==json,"Legacy format 3 still restores custom profiles");

    auto fields = organism_codec::Json::object();
    organism_codec::encodeFields(fields,glider,true);
    require(fields.at("organismProfile")=="glider" &&
        organism_codec::decodeFields(fields,profile,true).injuries.downed(),
        "A game save can reference the profile chosen by the game");
    fields.erase("organismProfile");
    require(organism_codec::decodeFields(fields,profile,true).injuries.bloodVolume()==1164,
        "Older embedded records can use the profile selected by the game");
    fields["organismProfile"] = "different";
    bool rejected = false;
    try { organism_codec::decodeFields(fields,profile,true); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected,"Reject a saved state belonging to a different profile");

    const auto builtin = State{}.profile().definition();
    const Profile sameId(builtin);
    organism_codec::encodeFields(fields,physiology::Organism{sameId},true);
    require(organism_codec::decodeFields(fields,sameId,true).alive(),
        "Use the explicitly supplied profile even if it shares a legacy ID");
    rejected = false;
    try { organism_codec::decodeFields(fields,Body::Humanoid,Resilience::Normal,true); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected,"Built-in decoding cannot silently substitute for an explicitly saved custom profile");

    auto invalidJson = organism_codec::Json::parse(json);
    invalidJson["profile"]["parts"][1]["id"] = static_cast<int>(leftWing);
    require(!organism_codec::decode(invalidJson.dump(),error),"Reject duplicate body part IDs in JSON");
    invalidJson = organism_codec::Json::parse(json);
    invalidJson["profile"]["blood"]["capacityMl"] = 1000;
    require(!organism_codec::decode(invalidJson.dump(),error),"Reject blood volume outside custom capacity");
    invalidJson = organism_codec::Json::parse(json);
    invalidJson["profile"]["trauma"]["lethal"] = 2;
    require(!organism_codec::decode(invalidJson.dump(),error),"Reject inconsistent trauma thresholds");
    invalidJson = organism_codec::Json::parse(json);
    invalidJson["profile"]["recovery"]["treatedRate"] = 0;
    require(!organism_codec::decode(invalidJson.dump(),error),"Reject invalid wound recovery settings");

    definition = gliderDefinition();
    definition.blood.regenerationFractionPerSecond = 0;
    definition.trauma.movementTrauma.reset();
    definition.trauma.movementWoundSeverity = Severity::Serious;
    State slow{Profile(definition)};
    require(slow.restoreBloodVolume(700) && slow.downed() && std::isinf(slow.nextBloodEventSeconds()),
        "A disabled regeneration rate has no recovery deadline");
    slow.advanceBlood(100);
    require(slow.bloodVolume()==700 && slow.restoreBloodVolume(1200) &&
        slow.applyDamage(rightWing,8) && slow.downed(),"Custom movement wound limits work at full blood volume");
}

void activeEffects() {
    using physiology::ActiveEffect;
    using physiology::WholeBody;
    physiology::Organism unit;
    require(unit.injuries.applyDamage(Part::Arm,12) && unit.injuries.applyDamage(Part::Leg,4),
        "Prepare impairment on two different parts");
    require(unit.addEffect({"arm-dose","analgesia",Part::Arm,2,{{"relief",.5}}}) &&
        unit.addEffect({"both-dose","analgesia",std::vector{Part::Arm,Part::Leg},1,{{"relief",.75}}}) &&
        unit.addEffect({"general-dose","analgesia",WholeBody{},3,{{"relief",.25}}}),"Apply all three scopes");
    require(std::abs(unit.armMultiplier()-.9375)<1e-9 && std::abs(unit.movementMultiplier()-.975)<1e-9 &&
        unit.nextEffectEventSeconds()==1,"Use the strongest overlapping relief separately on each part");
    require(!unit.addEffect({"arm-dose","unrelated",WholeBody{},std::nullopt,{}}) &&
        !unit.addEffect({"infinite-dose","analgesia",WholeBody{},std::nullopt,{{"relief",1.0}}}) &&
        !unit.addEffect({"bad-dose","analgesia",WholeBody{},1,{{"relief",2.0}}}) &&
        !unit.addEffect({"bad-target","custom",std::vector{Part::Leg,Part::Leg},1,{}}) &&
        !unit.addEffect({"missing-target","custom",leftWing,1,{}}),"Reject duplicate IDs, invalid relief and missing or duplicate target parts");

    double activeInterval = 0;
    unit.advanceEffects(1.5,[&](const ActiveEffect& effect,double elapsed) {
        if (effect.id=="both-dose") {
            activeInterval = elapsed;
            require(unit.addEffect({"marker","custom.marker",Part::Torso,5,{}}),"Effects can create follow-up effects");
        }
    });
    require(activeInterval==1 && !unit.findEffect("both-dose") && unit.findEffect("marker")->remainingSeconds==5 &&
        std::abs(unit.armMultiplier()-.875)<1e-9 && std::abs(unit.movementMultiplier()-.925)<1e-9 &&
        unit.nextEffectEventSeconds()==.5,"Advance only active time and leave follow-ups for the next step");
    unit.advanceEffects(.5);
    require(!unit.findEffect("arm-dose") && std::abs(unit.armMultiplier()-.8125)<1e-9,
        "An expired targeted dose leaves just the whole-body dose");
    require(unit.removeEffect("general-dose") && !unit.removeEffect("general-dose") && unit.armMultiplier()==.75 &&
        unit.movementMultiplier()==.9,"Removing the last dose restores baseline penalties");

    const ActiveEffect marker{"marker","custom.marker",Part::Head,10,{{"strength",std::int64_t{3}}}};
    require(unit.updateEffect(marker) && unit.findEffect("marker")->affects(Part::Head) &&
        !unit.findEffect("marker")->affects(Part::Torso) &&
        !unit.updateEffect({"marker","another.type",Part::Head,10,{}}),"Update an effect while preserving its identity and type");
    for (int i=0; i<100; ++i) unit.advanceEffects(.1);
    require(unit.effects().empty() && std::isinf(unit.nextEffectEventSeconds()),"Exact and divided time both expire effects");

    require(unit.injuries.applyDamage(Part::Leg,20) && unit.injuries.downed() &&
        unit.addEffect({"strong-dose","analgesia",WholeBody{},2,{{"relief",1.0}}}) &&
        unit.movementMultiplier()==0,"Relief does not reverse physical incapacitation");
}

void effectJson() {
    using physiology::WholeBody;
    const Profile profile(gliderDefinition());
    physiology::Organism glider{profile};
    require(glider.addEffect({"glider.spark.1","game.spark",std::vector{leftWing,rightWing},std::nullopt,
            {{"active",true},{"count",std::int64_t{7}},{"intensity",.5},{"name",std::string("blue")}}}) &&
        glider.addEffect({"glider.temporary.1","game.temporary",WholeBody{},4,{}}),"Create a persistent and a timed effect");
    glider.advanceEffects(1);
    const auto json = organism_codec::encode(glider);
    const auto value = organism_codec::Json::parse(json);
    require(value.at("formatVersion")==4 && value.at("profileId")=="glider" &&
        value.at("effects")[0].at("type")=="game.spark" && value.at("effects")[0].at("target").at("parts")[0]==10,
        "Persist profile, type, effect and part IDs independently of list positions");

    std::string error;
    const auto copy = organism_codec::decode(json,error);
    require(copy && organism_codec::encode(*copy)==json && copy->nextEffectEventSeconds()==3 &&
        copy->findEffect("glider.spark.1")->parameters==glider.findEffect("glider.spark.1")->parameters,
        "Restore custom type IDs, part scope, all parameter types and the remaining lifetime");
    auto fields = organism_codec::Json::object();
    organism_codec::encodeFields(fields,glider,false);
    require(organism_codec::decodeFields(fields,profile,false).effects().size()==2,
        "World records include active effects even for organisms without nutrition");
    fields.erase("effects");
    require(organism_codec::decodeFields(fields,profile,false).effects().empty(),"Old world records start with an empty effect list");

    auto invalid = value;
    invalid["effects"].push_back(invalid["effects"][0]);
    require(!organism_codec::decode(invalid.dump(),error),"Reject duplicate effect IDs in a save");
    invalid = value;
    invalid["effects"][0]["target"]["parts"][0] = 999;
    require(!organism_codec::decode(invalid.dump(),error),"Reject unknown part IDs in a saved effect");
    invalid = value;
    invalid["effects"][0]["parameters"]["count"] = std::numeric_limits<std::uint64_t>::max();
    require(!organism_codec::decode(invalid.dump(),error),"Reject integers beyond the effect parameter range");
    invalid = value;
    invalid["effects"][1]["remainingSeconds"] = 0;
    require(!organism_codec::decode(invalid.dump(),error),"Reject expired entries in the active effect list");
    invalid = value;
    invalid["profileId"] = "wrong-glider";
    require(!organism_codec::decode(invalid.dump(),error),"Reject conflicting profile IDs");
    invalid = organism_codec::Json::parse(organism_codec::encode(physiology::Organism{}));
    invalid["profileId"] = "unknown";
    require(!organism_codec::decode(invalid.dump(),error),"Reject unknown built-in profile IDs");
}

void bodyActions() {
    using namespace physiology;
    const auto woundPresent = [](const Organism& organism,Part part) { return organism.injuries.hasWound(part); };
    const BodyActionDefinition easePain{"relieve",
        [](Organism& organism,Part part) { return organism.applyAnalgesia("relief.wing",part,4,.5); },woundPresent};
    const BodyActionCatalog catalog({
        easePain,
        {"mark",[](Organism& organism,Part part) { return organism.addEffect({"marked","custom.mark",part,1,{}}); },{}},
        {"failure",[](Organism& organism,Part) { organism.injuries.restoreBloodVolume(0); return false; },{}},
    });
    const Profile profile(gliderDefinition());
    Organism patient{profile};
    const BodyActionTarget target{"relieve",leftWing};
    require(!catalog.canApply(patient,target) && !catalog.canApply(patient,"mark",Part::Arm) &&
        !catalog.canApply(patient,"unregistered",leftWing),"Validate anatomy, eligibility and the action ID");
    require(catalog.apply(patient,{"mark",rightWing}) && patient.findEffect("marked")->affects(rightWing),
        "An independent action can target a healthy part without an eligibility rule");
    require(patient.injuries.applyDamage(leftWing,8,WoundKind::Open) && catalog.canApply(patient,target),
        "An eligible action can target a wounded custom part");
    const auto prepared = catalog.prepare(patient,target);
    require(prepared && !patient.findEffect("relief.wing") && prepared->analgesiaRemainingSeconds(leftWing)==4 &&
        prepared->analgesiaRemainingSeconds(rightWing)==0,"Prepare a localized result without changing the patient");
    require(!catalog.apply(patient,{"failure",leftWing}) && patient.injuries.bloodVolume()==1200,
        "A rejected action leaves the original organism unchanged");
    require(catalog.apply(patient,target) && patient.nextConditionEventSeconds()==1,"Forecast timed and physiological events");
    patient.advanceCondition(1);
    require(!patient.findEffect("marked") && patient.analgesiaRemainingSeconds(leftWing)==3 &&
        patient.injuries.bloodVolume()<1200,"Advance blood loss and effect timers together");
    require(catalog.apply(patient,target) && patient.effects().size()==1 &&
        patient.analgesiaRemainingSeconds(leftWing)==4,"Refreshing a dose preserves its identity");
    patient.injuries.restInBed(10000);
    require(!catalog.prepare(patient,target) && patient.analgesiaRemainingSeconds(leftWing)==4,
        "Recheck eligibility when preparing an action");
    require(patient.injuries.restoreBloodVolume(0) && !catalog.apply(patient,{"mark",leftWing}),
        "Dead organisms cannot receive body actions");

    auto json = organism_codec::encodeBodyActionTarget(target);
    require(json.at("id")=="relieve" && json.at("part")==static_cast<int>(leftWing) &&
        organism_codec::decodeBodyActionTarget(json,catalog)==target,"Round-trip stable action and custom part IDs");
    json["id"]="missing";
    bool rejected = false;
    try { organism_codec::decodeBodyActionTarget(json,catalog); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected,"Reject an unknown action in saved work");
    rejected = false;
    try { const BodyActionCatalog duplicate({easePain,easePain}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected,"Reject duplicate stable action IDs");
}

} // namespace

int main() {
    try {
        anatomyAndWounds();
        nutritionForecast();
        thresholdsAndJson();
        customProfile();
        activeEffects();
        effectJson();
        bodyActions();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
