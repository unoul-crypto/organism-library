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
