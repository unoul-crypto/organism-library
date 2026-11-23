#include "physiology/BodyActions.hpp"
#include "StableIds.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace colony::physiology {
namespace {

bool validTarget(const Organism& organism,const BodyActionDefinition& action,injury::Part part) {
    return organism.alive() && organism.injuries.anatomy().find(part) &&
        (!action.eligible || action.eligible(organism,part));
}

} // namespace

BodyActionCatalog::BodyActionCatalog(std::vector<BodyActionDefinition> definitions) : definitions_(std::move(definitions)) {
    std::set<std::string_view> ids;
    for (const auto& definition : definitions_) {
        if (!detail::validId(definition.id) || !definition.apply || !ids.insert(definition.id).second)
            throw std::invalid_argument("Invalid or duplicate body action definition.");
    }
}

const BodyActionDefinition* BodyActionCatalog::find(std::string_view id) const {
    const auto found = std::find_if(definitions_.begin(),definitions_.end(),[id](const auto& action) {
        return action.id == id;
    });
    return found == definitions_.end() ? nullptr : &*found;
}

bool BodyActionCatalog::canApply(const Organism& organism,std::string_view id,injury::Part part) const {
    const auto* action = find(id);
    return action && validTarget(organism,*action,part);
}

std::optional<Organism> BodyActionCatalog::prepare(const Organism& organism,const BodyActionTarget& target) const {
    const auto* action = find(target.id);
    if (!action || !validTarget(organism,*action,target.part)) return std::nullopt;
    auto result = organism;
    if (!action->apply(result,target.part)) return std::nullopt;
    return result;
}

bool BodyActionCatalog::apply(Organism& organism,const BodyActionTarget& target) const {
    auto result = prepare(organism,target);
    if (!result) return false;
    organism = std::move(*result);
    return true;
}

} // namespace colony::physiology
