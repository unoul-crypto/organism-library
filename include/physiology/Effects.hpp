#pragma once

#include "physiology/InjuryProfile.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace colony::physiology {

using EffectParameter = std::variant<bool,std::int64_t,double,std::string>;
using EffectParameters = std::map<std::string,EffectParameter,std::less<>>;
inline constexpr std::string_view analgesiaEffectType = "analgesia";

struct ActiveEffect {
    std::string id;
    std::string typeId;
    EffectTarget<injury::Part> target;
    // No duration means the effect remains active until explicitly removed.
    std::optional<double> remainingSeconds;
    EffectParameters parameters;

    [[nodiscard]] bool affects(injury::Part part) const;
};

// Reports the active portion of a step using the effect's state at its start.
using EffectTick = std::function<void(const ActiveEffect&,double)>;

} // namespace colony::physiology
