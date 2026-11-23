#include "physiology/viewer/OrganismViewer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace colony;
using namespace colony::physiology;
using namespace colony::physiology::viewer;

namespace {

constexpr auto wing = static_cast<injury::Part>(12);
constexpr auto antenna = static_cast<injury::Part>(19);
constexpr Rect viewport{0,0,600,320};
constexpr Color purple{150,80,210};

void require(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct RecordingCanvas final : Canvas {
    std::vector<std::pair<std::string,Rect>> labels;
    Rect lastOutline{};
    Rect lastPurpleMark{};
    int lineCount = 0;
    int purpleMarks = 0;
    void fill(Rect bounds,Color color,float) override {
        if (color.red!=purple.red || color.green!=purple.green || color.blue!=purple.blue) return;
        lastPurpleMark=bounds;
        ++purpleMarks;
    }
    void outline(Rect bounds,Color,float) override { lastOutline=bounds; }
    void line(float,float,float,float,float,Color) override { ++lineCount; }
    void text(Rect bounds,std::string_view value,int,Color) override {
        labels.emplace_back(value,bounds);
    }

    [[nodiscard]] bool shows(std::string_view label) const {
        return std::any_of(labels.begin(),labels.end(),[label](const auto& item) { return item.first==label; });
    }

    [[nodiscard]] Input click(std::string_view label) const {
        const auto match=std::find_if(labels.begin(),labels.end(),[label](const auto& item) { return item.first==label; });
        if (match==labels.end()) throw std::runtime_error("No visible control called "+std::string(label));
        const auto bounds=match->second;
        return {bounds.x+bounds.width/2,bounds.y+bounds.height/2,true,0};
    }
};

void reviewPartAndEffects() {
    injury::ProfileDefinition anatomy;
    anatomy.id="glider";
    anatomy.parts={{wing,"Wing",.8,true,false},{antenna,"Antenna",1,false,false}};
    Organism glider{injury::Profile(std::move(anatomy))};
    require(glider.injuries.applyDamage(wing,12,injury::WoundKind::Open),"Prepare a wound on the wing");
    require(glider.addEffect({"shared","strain",std::vector{wing,antenna},30,{}}) &&
        glider.addEffect({"whole","adaptation",WholeBody{},std::nullopt,{}}),"Prepare shared and whole-body effects");

    const BodyActionCatalog catalog({{"dose",[](Organism&,injury::Part) { return true; },
        [](const Organism& organism,injury::Part part) { return organism.injuries.hasWound(part); }}});
    Options options;
    options.layers={{"Anatomy",{{wing,{.1F,.15F,.75F,.27F}},{antenna,{.1F,.55F,.40F,.27F}}}},{"Parts",{}}};
    options.effectStyles={{"strain","Strain",purple}};
    options.actions.catalog=&catalog;
    options.actions.partButtons={{"dose","Inject analgesic",true}};
    options.actions.effectButtons=[](injury::Part,const ActiveEffect& effect) {
        return effect.id=="shared" ? std::vector<Button>{{"clear","Clear strain",true}} : std::vector<Button>{};
    };

    OrganismViewer viewer;
    RecordingCanvas canvas;
    const auto draw=[&](Input input={}) {
        canvas={};
        return viewer.draw(canvas,viewport,input,glider,options);
    };
    require(!draw() && canvas.shows("Whole body") && canvas.shows("adaptation") &&
        !canvas.shows("Strain") && canvas.lineCount==1 && canvas.purpleMarks==2,
        "The diagram draws wound and custom effect markers, and the overview lists whole-body effects");
    require(!draw({90,125,true,0}) && viewer.selectedPart()==wing && canvas.shows("Serious wound") &&
        canvas.shows("Strain") && canvas.shows("30s remaining  |  Multiple parts") && canvas.shows("adaptation"),
        "Selecting a mapped custom part reveals its wounds, shared effects and whole-body effects");
    const auto inject=draw(canvas.click("Inject analgesic"));
    require(inject && inject->target==BodyActionTarget{"dose",wing} && !inject->effectId && glider.effects().size()==2,
        "A part button sends the existing action ID and selected part without changing the organism");

    options.actions.partButtons.front().enabled=false;
    require(!draw(canvas.click("Inject analgesic")),"Disabled actions cannot be requested");
    const auto clearWing=draw(canvas.click("Clear strain"));
    require(clearWing && clearWing->target==BodyActionTarget{"clear",wing} && clearWing->effectId=="shared",
        "An effect button identifies the exact effect instance");
    require(!draw({90,230,true,0}) && viewer.selectedPart()==antenna && canvas.shows("Strain") &&
        !canvas.shows("Serious wound") && !canvas.shows("Inject analgesic"),
        "Another part shares the effect and has no actions that require a wound");
    const auto clearAntenna=draw(canvas.click("Clear strain"));
    require(clearAntenna && clearAntenna->target.part==antenna && clearAntenna->effectId==clearWing->effectId,
        "The same multi-part effect keeps a single ID across both selections");

    require(!draw(canvas.click("Parts")) && !viewer.selectedPart() && canvas.shows("Wing") && canvas.shows("Antenna"),
        "A second layer can use a list, with independent selection");
    require(!draw(canvas.click("Antenna")) && viewer.selectedPart()==antenna,"The fallback list accepts part clicks");
    require(!draw(canvas.click("Overview")) && !viewer.selectedPart(),"Return to organism-wide effects");
}

bool atRelativeBounds(Rect marker, Rect part, Rect expected) {
    const auto close = [](float left,float right) { return std::abs(left-right)<.001F; };
    return close(marker.x,part.x+expected.x*part.width) && close(marker.y,part.y+expected.y*part.height) &&
        close(marker.width,expected.width*part.width) && close(marker.height,expected.height*part.height);
}

void effectMarkerGeometry() {
    const auto part=injury::Part::Torso;
    Organism organism;
    require(organism.addEffect({"press","pressure",part,std::nullopt,{}}) &&
        organism.addEffect({"bruised","bruise",part,std::nullopt,{}}),"Add two effects to the same part");

    constexpr Rect customBounds{.14F,.22F,.31F,.35F};
    constexpr Rect bruiseBounds{.63F,.71F,.29F,.11F};
    std::optional<Rect> strokeBounds;
    Options options;
    options.layers={{"Anatomy",{{part,{.15F,.16F,.66F,.72F}}}}};
    options.effectStyles={
        {"pressure","Pressure",purple,MarkerGeometry{customBounds,
            [&](Canvas& canvas,Rect bounds,Color color) {
                strokeBounds=bounds;
                canvas.line(bounds.x,bounds.y,bounds.x+bounds.width,bounds.y+bounds.height,2,color);
                canvas.line(bounds.x+bounds.width,bounds.y,bounds.x,bounds.y+bounds.height,2,color);
            }}},
        {"bruise","Bruise",purple,MarkerGeometry{bruiseBounds,{}}},
    };
    RecordingCanvas canvas;
    OrganismViewer viewer;
    for (const Rect bounds : {viewport,Rect{50,80,720,410}}) {
        canvas={};
        strokeBounds.reset();
        require(!viewer.draw(canvas,bounds,{},organism,options) &&
            strokeBounds && atRelativeBounds(*strokeBounds,canvas.lastOutline,customBounds) && canvas.lineCount==2,
            "The viewer invokes a custom shape at the configured relative size and position");
        require(canvas.purpleMarks==1 && atRelativeBounds(canvas.lastPurpleMark,canvas.lastOutline,bruiseBounds),
            "Another effect uses its own geometry and the default shape");
    }

    options.effectStyles.front().geometry->normalizedBounds.x=1;
    canvas={};
    strokeBounds.reset();
    require(!viewer.draw(canvas,viewport,{},organism,options) && !strokeBounds && canvas.purpleMarks==1,
        "An invalid marker rectangle does not draw outside the part or affect other markers");
}

} // namespace

int main() {
    try {
        reviewPartAndEffects();
        effectMarkerGeometry();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
