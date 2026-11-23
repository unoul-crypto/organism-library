#pragma once

#include "physiology/BodyActions.hpp"

#include <nlohmann/json_fwd.hpp>

namespace colony::organism_codec {

using Json = nlohmann::ordered_json;

// Structured access for embedding an organism in a larger JSON document.
// The caller supplies the profile and chooses whether that record tracks nutrition.
void encodeFields(Json& value, const physiology::Organism& organism, bool includeNutrition);
physiology::Organism decodeFields(const Json& value, injury::Body body, injury::Resilience resilience, bool includeNutrition);
// A record with an explicit profile ID must match the supplied definition.
physiology::Organism decodeFields(const Json& value, const injury::Profile& profile, bool includeNutrition);

Json encodeInjuries(const injury::State& injuries);
injury::State decodeInjuries(const Json& value, injury::Body body, injury::Resilience resilience,
                             bool allowMissingOpenDamage = false);

Json encodeBodyActionTarget(const physiology::BodyActionTarget& target);
// Check the saved action ID and part ID. The caller can check the recipient's profile
// separately once the organism associated with this target has been loaded.
physiology::BodyActionTarget decodeBodyActionTarget(const Json& value,const physiology::BodyActionCatalog& catalog);

} // namespace colony::organism_codec
