#pragma once

#include "physiology/Organism.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace colony::organism_codec {

[[nodiscard]] std::string encode(const physiology::Organism& organism);
// Return no state and set error if the document is malformed or unsupported.
[[nodiscard]] std::optional<physiology::Organism> decode(std::string_view json, std::string& error);

} // namespace colony::organism_codec
