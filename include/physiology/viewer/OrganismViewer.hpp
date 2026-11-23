#pragma once

#include "physiology/BodyActions.hpp"

#include <cstdint>
#include <functional>

namespace colony::physiology::viewer {

struct Rect {
    float x, y, width, height;
};

struct Color {
    std::uint8_t red, green, blue, alpha = 255;
};

class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void fill(Rect bounds, Color color, float roundness) = 0;
    virtual void outline(Rect bounds, Color color, float roundness) = 0;
    virtual void line(float x1, float y1, float x2, float y2, float thickness, Color color) = 0;
    // Clip or shorten the label to its bounds using the application's font.
    virtual void text(Rect bounds, std::string_view label, int size, Color color) = 0;
};

struct Input {
    float x = 0;
    float y = 0;
    bool clicked = false;
    float wheel = 0;
};

struct PartRegion {
    injury::Part part;
    Rect normalizedBounds; // Coordinates in [0, 1] relative to the square diagram.
};

struct Layer {
    std::string label;
    std::vector<PartRegion> regions; // Empty uses a selectable list of all parts.
};

struct MarkerGeometry {
    Rect normalizedBounds{0,0,1,1}; // Relative to the part's diagram region or the row's marker area.
    // Receives the marker's resolved drawing bounds. Empty uses a rounded rectangle.
    std::function<void(Canvas&,Rect,Color)> draw;
};

struct EffectStyle {
    std::string typeId;
    std::string label;
    std::optional<Color> marker;
    std::optional<MarkerGeometry> geometry = std::nullopt; // Empty uses automatic marker placement.
};

struct Button {
    std::string id;
    std::string label;
    bool enabled = true;
};

struct Actions {
    const BodyActionCatalog* catalog = nullptr;
    std::vector<Button> partButtons;
    std::function<std::vector<Button>(injury::Part,const ActiveEffect&)> effectButtons;
};

struct ActionRequest {
    BodyActionTarget target; // The action ID and the selected body part.
    std::optional<std::string> effectId; // Set for a button supplied by effectButtons.
};

struct Options {
    // An empty list shows the built-in Anatomy tab, with a list for custom profiles.
    std::vector<Layer> layers;
    std::vector<EffectStyle> effectStyles;
    Actions actions;
};

class OrganismViewer {
public:
    [[nodiscard]] std::optional<injury::Part> selectedPart() const { return selectedPart_; }
    void reset();

    // Render one frame and report at most one action. The caller owns the organism
    // and decides when and how to execute any reported action.
    [[nodiscard]] std::optional<ActionRequest> draw(Canvas& canvas, Rect bounds, const Input& input,
        const Organism& organism, const Options& options = {});

private:
    std::optional<injury::Part> selectedPart_;
    std::size_t layer_ = 0;
    std::size_t partScroll_ = 0;
    std::size_t detailScroll_ = 0;
};

} // namespace colony::physiology::viewer
