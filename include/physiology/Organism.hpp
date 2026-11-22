#pragma once

#include "physiology/Injuries.hpp"
#include "physiology/Nutrition.hpp"

namespace colony::physiology {

struct Organism {
    Organism() = default;
    explicit Organism(injury::Body body, injury::Resilience resilience = injury::Resilience::Normal)
        : injuries(body,resilience) {}

    injury::State injuries;
    Nutrition nutrition;

    [[nodiscard]] bool alive() const { return injuries.alive() && !nutrition.starvation.lethal(); }
};

} // namespace colony::physiology
