#include "physiology/BodyActions.hpp"
#include "StableIds.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace colony::physiology {
namespace {

template<class Definition>
const Definition* findDefinition(const std::vector<Definition>& definitions,std::string_view id) {
    const auto found = std::find_if(definitions.begin(),definitions.end(),[id](const auto& action) { return action.id == id; });
    return found == definitions.end() ? nullptr : &*found;
}

template<class Operation>
bool withEligibleSubject(const BodyActionCatalog& catalog,const Organism& organism,
    const BodyActionTarget& target,Operation operation) {
    if (!organism.alive() || !organism.injuries.anatomy().find(target.part)) return false;
    const auto run = [&](const auto* definition,const auto& subject) {
        return definition && (!definition->eligible || definition->eligible(organism,subject)) &&
            operation(*definition,subject);
    };
    if (const auto* woundId = std::get_if<injury::WoundId>(&target.subject)) {
        const auto* wound = organism.injuries.findWound(*woundId);
        return wound && wound->part == target.part && run(catalog.findWoundAction(target.id),*wound);
    }
    if (const auto* effectId = std::get_if<std::string>(&target.subject)) {
        const auto* effect = organism.findEffect(*effectId);
        return effect && effect->affects(target.part) && run(catalog.findEffectAction(target.id),*effect);
    }
    return std::holds_alternative<std::monostate>(target.subject) && run(catalog.find(target.id),target.part);
}

} // namespace

WoundActionDefinition bandageWoundAction() {
    return {bandageWoundActionId,
        [](Organism& organism,const injury::Wound& wound) { return organism.injuries.treat(wound.id); },
        [](const Organism&,const injury::Wound& wound) { return !wound.treated; }};
}

BodyActionCatalog::BodyActionCatalog(std::vector<BodyActionDefinition> definitions,
    std::vector<WoundActionDefinition> woundActions,std::vector<EffectActionDefinition> effectActions)
    : definitions_(std::move(definitions)), woundActions_(std::move(woundActions)), effectActions_(std::move(effectActions)) {
    std::set<std::string_view> ids;
    const auto validate = [&](const auto& actions) {
        for (const auto& definition : actions)
            if (!detail::validId(definition.id) || !definition.apply || !ids.insert(definition.id).second)
                throw std::invalid_argument("Invalid or duplicate body action definition.");
    };
    validate(definitions_);
    validate(woundActions_);
    validate(effectActions_);
}

const BodyActionDefinition* BodyActionCatalog::find(std::string_view id) const {
    return findDefinition(definitions_,id);
}

const WoundActionDefinition* BodyActionCatalog::findWoundAction(std::string_view id) const {
    return findDefinition(woundActions_,id);
}

const EffectActionDefinition* BodyActionCatalog::findEffectAction(std::string_view id) const {
    return findDefinition(effectActions_,id);
}

bool BodyActionCatalog::recognizes(const BodyActionTarget& target) const {
    if (const auto* woundId = std::get_if<injury::WoundId>(&target.subject))
        return *woundId != 0 && findWoundAction(target.id);
    if (const auto* effectId = std::get_if<std::string>(&target.subject))
        return detail::validId(*effectId) && findEffectAction(target.id);
    return std::holds_alternative<std::monostate>(target.subject) && find(target.id);
}

bool BodyActionCatalog::canApply(const Organism& organism,std::string_view id,injury::Part part) const {
    const auto* action = find(id);
    return action && organism.alive() && organism.injuries.anatomy().find(part) &&
        (!action->eligible || action->eligible(organism,part));
}

bool BodyActionCatalog::canApply(const Organism& organism,const BodyActionTarget& target) const {
    return withEligibleSubject(*this,organism,target,[](const auto&,const auto&) { return true; });
}

std::optional<Organism> BodyActionCatalog::prepare(const Organism& organism,const BodyActionTarget& target) const {
    std::optional<Organism> result;
    if (!withEligibleSubject(*this,organism,target,[&](const auto& action,const auto& subject) {
        result.emplace(organism);
        return action.apply(*result,subject);
    })) return std::nullopt;
    return result;
}

bool BodyActionCatalog::apply(Organism& organism,const BodyActionTarget& target) const {
    auto result = prepare(organism,target);
    if (!result) return false;
    organism = std::move(*result);
    return true;
}

} // namespace colony::physiology
