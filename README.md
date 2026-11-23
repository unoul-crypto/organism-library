# Organism Library

A C++17 simulation library for anatomy, injury, blood loss, nutrition, and
body-part actions. It includes configurable injury profiles, active effects,
a versioned JSON codec, and an optional viewer.

## Build and test

Use CMake 3.21 or newer and a C++17 compiler. CMake uses an available
nlohmann/json package or fetches the pinned dependency on first configuration.

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Add `-DORGANISM_BUILD_VIEWER=ON` to the configure command to build the viewer and
its renderer-independent `organism_viewer` test.

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

Standalone JSON format 5 uses a stable string `profileId` for built-in and
custom profiles. It embeds the definition of each custom profile. The codec
also reads formats 1 through 4. For a world save, `encodeFields` writes the custom profile
ID, and `decodeFields` accepts the profile supplied by the game. Older world
records without that ID use the supplied profile. Keep a profile ID and its
part IDs stable across compatible releases, or migrate the affected saved state
when changing its rules.

## Active effects

An effect has an instance ID unique within the organism, a type ID shared by
instances of that effect, a target, and a map of parameters. Targets can be the
whole body, one part, or several parts. Parameters support booleans, signed
64-bit integers, finite floating-point numbers, and strings. Effect IDs, type
IDs, and parameter names use letters AвЂ“Z, digits, dots, hyphens, or underscores.

```cpp
colony::physiology::Organism patient;
patient.injuries.applyDamage(colony::injury::Part::Arm, 12);
patient.applyAnalgesia("bandage.analgesic.1", colony::injury::Part::Arm, 30, .5);
const double workMultiplier = patient.armMultiplier();
```

`analgesia` is a timed effect. A `relief` of 0.5 halves the penalty on the targeted
parts. The strongest overlapping dose applies to each part. Physical
incapacitation still prevents movement.
`applyAnalgesia` starts or refreshes a timed dose by ID.
`analgesiaRemainingSeconds(part)` reports the longest remaining dose affecting
that part, or zero when there is no dose.

Use `addEffect`, `updateEffect`, and `removeEffect` to manage the list.
A missing duration makes a generic effect persistent. Call `advanceEffects`
with simulation seconds and include `nextEffectEventSeconds` in the event
horizon when splitting simulation steps at expiry. An optional tick handler
receives the effect state at the start of the step and the time it was active.
The timers reach the end of the step before handlers run. Changes made by a
handler take effect on the next step.
Call `advanceCondition` to advance blood volume and effects in the same step.
`nextConditionEventSeconds` returns the earlier of their event deadlines.

Standalone format 4 introduced effect identity, type, scope, parameters, and remaining time.
World records embed the same effect list when it is present. Existing saves
load with an empty list. Game-defined effect types retain their data across
JSON round trips and can provide their behavior through the tick handler.

## Body actions

A `BodyActionCatalog` holds operations defined by the application. Each
definition has a unique string ID, an operation, and an optional eligibility
check. The catalog checks that the organism is alive and the selected part
exists in its profile. A custom profile can supply its own part IDs.

```cpp
#include "physiology/BodyActions.hpp"

using namespace colony::physiology;
using colony::injury::Part;

int main() {
    const BodyActionCatalog actions({
        {"relieve",
            [](Organism& organism, Part part) {
                return organism.applyAnalgesia("local.relief", part, 30, .5);
            },
            [](const Organism& organism, Part part) {
                return organism.injuries.hasWound(part);
            }},
    });
    const BodyActionTarget target{"relieve", Part::Arm};
    Organism patient;
    patient.injuries.applyDamage(Part::Arm, 12);
    return actions.apply(patient, target) ? 0 : 1;
}
```

`canApply` checks eligibility. `apply` checks it again and commits the result
only if the operation succeeds. To coordinate an inventory change or other
application state, call `prepare` and adopt the returned organism after that
change succeeds. Operations should mutate only the supplied organism.

`encodeBodyActionTarget` and `decodeBodyActionTarget` in
`physiology/OrganismJson.hpp` let applications save pending jobs by action and
part ID. Keep the action ID stable when changing its presentation.

### Wound and effect actions

Register `WoundActionDefinition` and `EffectActionDefinition` in the second
and third catalog arguments. The catalog checks that the instance exists and
affects the specified part before applying the action. For example, the
library's `bandageWoundAction()` treats one untreated wound. The host can wrap
`prepare` with its own supply and work rules.

```cpp
#include "physiology/BodyActions.hpp"

using namespace colony::physiology;

bool bandage(Organism& patient, colony::injury::Part part, colony::injury::WoundId id) {
    const BodyActionCatalog actions({}, {bandageWoundAction()});
    return actions.apply(patient, {bandageWoundActionId, part, id});
}
```

`BodyActionTarget::subject` holds a wound's numeric ID or an active effect's
string ID. Leave it empty for a part action. The target codec stores instance
IDs as `woundId` or `effectId`. When queuing effect actions, keep each effect ID
associated with the same logical instance.

Format 5 stores wound IDs and the next ID as decimal strings. Loading older
saves assigns a distinct ID to each wound. New wounds receive fresh IDs after
healing and reloading. A hit that merges wounds on the same part keeps the
first wound's ID. The other IDs no longer resolve to wounds.

### Wound recovery

`WoundRecoveryContext` applies an activity multiplier and optional per-wound
factors to the profile's treated or untreated recovery rate. The default
context uses the normal bed-rest rates. A factor of 1 leaves the rate as it
is. Zero, a negative value, or a non-finite value pauses recovery. Paused wounds
retain their remaining work and have no predicted recovery event.

Use the same context for `injury::State::advanceRecovery` and
`nextRecoverySeconds`. Factor callbacks may use a wound's stable ID to read
the app's conditions or a related active effect. Keep these inputs stable
over a simulation step and recompute the rate when the conditions change.
`restInBed` uses the default context, and `Wound::recoverySeconds(context)`
reports the time for a single wound under the given conditions.

## Organism viewer

The viewer is opt-in. Set `ORGANISM_BUILD_VIEWER` before adding the dependency.
The viewer target links the simulation library.

```cmake
set(ORGANISM_BUILD_VIEWER ON)
add_subdirectory(organism-library)
target_link_libraries(your_target PRIVATE colony_organism_viewer)
```

Include `physiology/viewer/OrganismViewer.hpp` and implement the four `Canvas`
operations using the host's renderer. Keep one `OrganismViewer` per open panel
and call `draw` each frame. Pass the pointer position and panel bounds in the
same coordinate space, a click on release, and the wheel delta for that frame.

With default options, the viewer displays an Anatomy tab. Built-in humanoid and
quadruped profiles have diagrams. Custom profiles have a selectable list. To
provide a diagram, use `Options::layers` to put part IDs in normalized rectangles.
Multiple regions can share a part ID, as in the built-in paired limb groups.
An empty layer displays all parts in a scrollable list.

Wounds appear as red slashes. Each `Options::effectStyles` entry matches an
active effect's `typeId`. Its label appears in the details panel, and its
optional color adds a marker to the diagram. The details panel shows wounds
and effects affecting the selected part. The overview lists whole-body effects.

Set an effect style's `geometry` to control its marker. The rectangle contains
`x`, `y`, width, and height as fractions of the part's diagram region, or of
the marker area in a list row. Keep the entire rectangle within `[0, 1]`.
The optional drawing function receives the resulting screen rectangle and the
marker's color. Use the rectangle to draw a shape that scales with the diagram.

```cpp
#include "physiology/viewer/OrganismViewer.hpp"

namespace viewer = colony::physiology::viewer;

viewer::Options strainViewerOptions() {
    viewer::Options options;
    options.effectStyles.push_back({
        "strain", "Strain", viewer::Color{172, 112, 215},
        viewer::MarkerGeometry{
            {.15F, .20F, .35F, .25F},
            [](viewer::Canvas& canvas, viewer::Rect box, viewer::Color color) {
                canvas.line(box.x, box.y + box.height, box.x + box.width, box.y, 2, color);
            }
        }
    });
    return options;
}
```

A color alone uses the original automatic placement. Geometry without a
drawing function uses a rounded rectangle at the specified position.

Provide the catalog and `partButtons` for actions on the selected part. Add
`instanceButtons` for registered actions beside individual wounds or active
effects. The viewer displays a button when the catalog accepts its exact
target. The host can disable buttons when supplies are unavailable.
`effectButtons` can supply additional host-provided buttons below active
effects. Each request carries the instance ID in `target.subject` when
applicable. Call `reset` when the viewer switches to another organism.
