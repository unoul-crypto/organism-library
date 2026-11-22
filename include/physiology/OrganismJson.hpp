#pragma once

#include "physiology/Organism.hpp"

#include <nlohmann/json_fwd.hpp>

namespace colony::organism_codec {

using Json = nlohmann::ordered_json;

// Structured access for embedding an organism in a larger JSON document.
// The caller supplies the profile and chooses whether that record tracks nutrition.
void encodeFields(Json& value, const physiology::Organism& organism, bool includeNutrition);
physiology::Organism decodeFields(const Json& value, injury::Body body, injury::Resilience resilience, bool includeNutrition);

Json encodeInjuries(const injury::State& injuries);
injury::State decodeInjuries(const Json& value, injury::Body body, injury::Resilience resilience,
                             bool allowMissingOpenDamage = false);

} // namespace colony::organism_codec
