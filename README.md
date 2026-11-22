# Organism Library

A C++17 simulation library for anatomy, injury, blood loss, and nutrition.
It includes humanoid and quadruped injury profiles and a versioned JSON codec
for saving and restoring an organism.

## Build and test

Use CMake 3.21 or newer and a C++17 compiler. CMake uses an available
nlohmann/json package or fetches the pinned dependency on first configuration.

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Use in a CMake project

```cmake
add_subdirectory(organism-library)
target_link_libraries(your_target PRIVATE colony_organism)
```

```cpp
#include "physiology/OrganismCodec.hpp"

#include <string>

int main() {
    colony::physiology::Organism patient;
    patient.injuries.applyDamage(colony::injury::Part::Arm, 8,
                                 colony::injury::WoundKind::Open);
    patient.injuries.advanceBlood(1.0);
    patient.nutrition.advance(1.0);

    const std::string json = colony::organism_codec::encode(patient);
    std::string error;
    const auto restored = colony::organism_codec::decode(json, error);
    return restored ? 0 : 1;
}
```

`OrganismCodec.hpp` provides the string API. Use `OrganismJson.hpp` when
embedding the organism's fields in a larger nlohmann/json document.

## Custom profiles

A game can configure a profile once and share it across its organisms.
Each part has a stable nonnegative integer ID and a cumulative hit range.
The final part ends the range at 1. Mark parts that contribute to movement or
arm function to apply the corresponding penalties when those parts are wounded.

```cpp
using namespace colony;
constexpr auto wing = static_cast<injury::Part>(10);
constexpr auto hull = static_cast<injury::Part>(11);

injury::ProfileDefinition definition;
definition.id = "glider";
definition.parts = {{wing, "Wing", .4, true}, {hull, "Hull", 1}};
definition.blood.capacityMl = 1200;
definition.blood.regenerationFractionPerSecond = .002;
definition.trauma.movementWoundSeverity = injury::Severity::Serious;
definition.trauma.seriousWoundDamage = 8;
definition.trauma.criticalWoundDamage = 16;
definition.recovery.bedRestSeconds = {10, 30, 60};
definition.recovery.untreatedRate = .25;
definition.recovery.treatedRate = 2;

const injury::Profile gliderProfile(definition);
physiology::Organism glider(gliderProfile);
```

Blood parameters set capacity, the incapacitation fraction, regeneration and
loss per point of open damage. Trauma parameters set wound severity, general
incapacitation, death, and optional movement limits. Each whole serious-wound
damage threshold in a wound contributes one trauma point. Recovery parameters
set bed rest work for each wound severity and its rates before and after
treatment.

Standalone JSON stores custom profiles with their definitions in format 3.
Built-in profiles keep the existing format 2 representation, and the codec
reads formats 1 and 2. For a world save, `encodeFields` writes the custom profile
ID, and `decodeFields` accepts the profile supplied by the game. Older world
records without that ID use the supplied profile. Keep a profile ID and its
part IDs stable across compatible releases, or migrate the affected saved state
when changing its rules.
