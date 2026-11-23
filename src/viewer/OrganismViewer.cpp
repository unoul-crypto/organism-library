#include "physiology/viewer/OrganismViewer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace colony::physiology::viewer {
namespace {

constexpr Color surface{23,32,31};
constexpr Color card{33,45,43};
constexpr Color border{66,88,82};
constexpr Color outline{105,128,120};
constexpr Color ink{231,237,226};
constexpr Color muted{164,183,175};
constexpr Color accent{237,199,114};
constexpr Color woundInk{245,116,113};
constexpr Color reliefInk{104,213,183};
constexpr Color bodyFill{63,89,83};
constexpr Color selectedFill{103,104,76};
constexpr float gap = 10;

bool contains(Rect rect, const Input& input) {
    return input.x >= rect.x && input.x < rect.x+rect.width && input.y >= rect.y && input.y < rect.y+rect.height;
}

void label(Canvas& canvas, Rect bounds, std::string_view value, int size, Color color = ink) {
    if (bounds.width > 0 && bounds.height >= size) canvas.text(bounds,value,size,color);
}

bool button(Canvas& canvas, Rect bounds, const Input& input, std::string_view title, bool enabled, bool selected = false) {
    const bool hovered = enabled && contains(bounds,input);
    canvas.fill(bounds,selected ? selectedFill : hovered ? bodyFill : card,.24F);
    canvas.outline(bounds,selected ? accent : hovered ? outline : border,.24F);
    label(canvas,{bounds.x+9,bounds.y+(bounds.height-14)/2,bounds.width-18,14},title,14,enabled ? ink : muted);
    return hovered && input.clicked;
}

std::string number(double value, int decimals = 0) {
    char text[32];
    std::snprintf(text,sizeof(text),"%.*f",decimals,value);
    return text;
}

const Layer& anatomyLayer(injury::Body body) {
    using injury::Part;
    static const Layer humanoid{"Anatomy",{
        {Part::Head,{.395F,.055F,.21F,.215F}},
        {Part::Torso,{.345F,.29F,.31F,.325F}},
        {Part::Arm,{.17F,.30F,.14F,.34F}},
        {Part::Arm,{.69F,.30F,.14F,.34F}},
        {Part::Leg,{.345F,.64F,.135F,.315F}},
        {Part::Leg,{.52F,.64F,.135F,.315F}},
    }};
    static const Layer quadruped{"Anatomy",{
        {Part::Leg,{.265F,.55F,.13F,.31F}},
        {Part::Leg,{.62F,.55F,.13F,.31F}},
        {Part::Torso,{.16F,.31F,.63F,.29F}},
        {Part::Head,{.75F,.24F,.20F,.22F}},
    }};
    static const Layer custom{"Anatomy",{}};
    switch (body) {
    case injury::Body::Humanoid: return humanoid;
    case injury::Body::Quadruped: return quadruped;
    default: return custom;
    }
}

bool validRegion(Rect bounds) {
    return bounds.x >= 0 && bounds.y >= 0 && bounds.width > 0 && bounds.height > 0 &&
        bounds.x+bounds.width <= 1 && bounds.y+bounds.height <= 1;
}

Rect resolveBounds(Rect area, Rect normalized) {
    return {area.x+normalized.x*area.width,area.y+normalized.y*area.height,
        normalized.width*area.width,normalized.height*area.height};
}

const EffectStyle& styleFor(const ActiveEffect& effect, const std::vector<EffectStyle>& styles) {
    const auto custom = std::find_if(styles.begin(),styles.end(),[&](const auto& style) {
        return style.typeId == effect.typeId;
    });
    if (custom != styles.end()) return *custom;
    static const EffectStyle analgesia{std::string(analgesiaEffectType),"Pain relief",reliefInk};
    static const EffectStyle generic{};
    return effect.typeId == analgesiaEffectType ? analgesia : generic;
}

// Keep the scroll offset valid when the underlying parts or effects change.
std::size_t visibleEnd(Canvas& canvas, Rect area, const Input& input, std::size_t count, float stride, std::size_t& scroll) {
    const auto capacity = static_cast<std::size_t>(std::max(0.F,std::floor(area.height/stride)));
    const auto maxScroll = count > capacity ? count-capacity : 0;
    scroll = std::min(scroll,maxScroll);
    if (contains(area,input)) {
        if (input.wheel < 0 && scroll < maxScroll) ++scroll;
        if (input.wheel > 0 && scroll > 0) --scroll;
    }
    if (maxScroll && capacity) {
        const float thumb = std::max(8.F,area.height*(static_cast<float>(capacity)/count));
        const float y = area.y+(area.height-thumb)*(static_cast<float>(scroll)/maxScroll);
        canvas.fill({area.x+area.width-3,y,3,thumb},outline,.5F);
    }
    return std::min(count,scroll+capacity);
}

struct Region {
    injury::Part part;
    Rect bounds;
};

std::vector<Region> diagramRegions(Rect area, const Layer& layer, const injury::State& injuries) {
    const float size = std::min(area.width,area.height)-12;
    const Rect square{area.x+(area.width-size)/2,area.y+(area.height-size)/2,size,size};
    std::vector<Region> result;
    for (const auto& region : layer.regions) {
        const auto box = region.normalizedBounds;
        if (validRegion(box) && injuries.anatomy().find(region.part))
            result.push_back({region.part,resolveBounds(square,box)});
    }
    return result;
}

void drawMarks(Canvas& canvas, Rect rect, injury::Part part, const Organism& organism, const Options& options) {
    const auto wounds = std::count_if(organism.injuries.wounds().begin(),organism.injuries.wounds().end(),
        [part](const auto& wound) { return wound.part == part; });
    const auto slashes = std::min<decltype(wounds)>(wounds,3);
    const float width = std::min(rect.width*.39F,19.F);
    const float spacing = std::min(rect.height*.13F,8.F);
    for (int index=0; index<slashes; ++index) {
        const float x = rect.x+rect.width*.5F;
        const float y = rect.y+rect.height*.48F+(index-(slashes-1)*.5F)*spacing;
        canvas.line(x-width*.5F,y+spacing*.32F,x+width*.5F,y-spacing*.32F,2.6F,woundInk);
    }
    const float markerWidth = std::min(7.F,rect.width*.19F);
    float markerX = rect.x+rect.width*.19F;
    for (const auto& effect : organism.effects()) {
        if (!effect.affects(part)) continue;
        const auto& style = styleFor(effect,options.effectStyles);
        if (!style.marker) continue;
        if (const auto& geometry = style.geometry) {
            if (!validRegion(geometry->normalizedBounds)) continue;
            const auto bounds = resolveBounds(rect,geometry->normalizedBounds);
            if (geometry->draw) geometry->draw(canvas,bounds,*style.marker);
            else canvas.fill(bounds,*style.marker,.6F);
            continue;
        }
        if (markerX+markerWidth > rect.x+rect.width*.85F) continue;
        canvas.fill({markerX,rect.y+rect.height*.77F,markerWidth,std::min(5.F,rect.height*.15F)},*style.marker,.6F);
        markerX += markerWidth+3;
    }
}

struct Row {
    std::string title;
    std::string detail;
    Color color = muted;
    std::optional<Button> action;
    BodyActionSubject subject;
};

void addInstanceRow(std::vector<Row>& rows, Row row, const Organism& organism, injury::Part part,
    const Actions& actions) {
    const auto cardIndex = rows.size();
    rows.push_back(std::move(row));
    if (!actions.catalog) return;
    for (const auto& action : actions.instanceButtons) {
        auto& entry = rows[cardIndex];
        if (!actions.catalog->canApply(organism,{action.id,part,entry.subject})) continue;
        if (!entry.action) entry.action = action;
        else rows.push_back({{}, {}, accent, Button{action.id,action.label+" / "+entry.title,action.enabled},entry.subject});
    }
}

std::vector<Row> details(const Organism& organism, std::optional<injury::Part> part, const Options& options) {
    std::vector<Row> rows;
    if (part) {
        const auto& actions = options.actions;
        for (const auto& action : actions.partButtons)
            if (actions.catalog && actions.catalog->canApply(organism,action.id,*part))
                rows.push_back({{}, {}, accent, action, {}});
        constexpr const char* severity[] = {"Mild wound","Serious wound","Critical wound"};
        for (const auto& wound : organism.injuries.wounds()) {
            if (wound.part != *part) continue;
            auto detail = std::string(wound.treated ? "Treated" : "Untreated")+" | "+number(wound.damage,1)+
                " dmg | "+number(std::ceil(wound.recoverySeconds()))+"s rest";
            if (wound.bloodLossRate() > 0) detail += " | "+number(wound.bloodLossRate(),1)+" mL/s";
            addInstanceRow(rows,{severity[static_cast<int>(wound.severity)],std::move(detail),woundInk,{},wound.id},
                organism,*part,actions);
        }
    }
    bool hasEffects = std::any_of(organism.injuries.wounds().begin(),organism.injuries.wounds().end(),
        [&](const auto& wound) { return part && wound.part == *part; });
    for (const auto& effect : organism.effects()) {
        const bool wholeBody = std::holds_alternative<WholeBody>(effect.target);
        if (part ? !effect.affects(*part) : !wholeBody) continue;
        hasEffects = true;
        const auto& style = styleFor(effect,options.effectStyles);
        auto detail = effect.remainingSeconds ? number(std::ceil(*effect.remainingSeconds))+"s remaining" : std::string("Active");
        if (wholeBody) detail += "  |  Whole body";
        else if (std::holds_alternative<std::vector<injury::Part>>(effect.target)) detail += "  |  Multiple parts";
        Row effectRow{style.label.empty() ? effect.typeId : style.label,std::move(detail),style.marker.value_or(muted),{},effect.id};
        if (part) addInstanceRow(rows,std::move(effectRow),organism,*part,options.actions);
        else rows.push_back(std::move(effectRow));
        if (part && options.actions.effectButtons) {
            for (auto action : options.actions.effectButtons(*part,effect))
                if (!action.id.empty()) rows.push_back({{}, {}, accent, std::move(action), effect.id});
        }
    }
    if (!hasEffects) rows.push_back({part ? "No wounds or active effects" : "No whole-body effects",{}});
    return rows;
}

} // namespace

void OrganismViewer::reset() {
    selectedPart_.reset();
    layer_ = partScroll_ = detailScroll_ = 0;
}

std::optional<ActionRequest> OrganismViewer::draw(Canvas& canvas, Rect bounds, const Input& input,
    const Organism& organism, const Options& options) {
    if (bounds.width < 240 || bounds.height < 145) {
        label(canvas,bounds,"Expand the panel to view anatomy",14,muted);
        return std::nullopt;
    }

    const auto& injuries = organism.injuries;
    const auto& profile = injuries.profile();
    if (selectedPart_ && !profile.part(*selectedPart_)) {
        selectedPart_.reset();
        detailScroll_ = 0;
    }
    const bool customLayers = !options.layers.empty();
    const std::size_t layerCount = customLayers ? options.layers.size() : 1;
    layer_ = std::min(layer_,layerCount-1);
    const float tabWidth = std::min(122.F,bounds.width/layerCount);
    for (std::size_t index=0; index<layerCount; ++index) {
        if (button(canvas,{bounds.x+index*tabWidth,bounds.y,tabWidth-4,25},input,
                customLayers ? options.layers[index].label : "Anatomy",true,index == layer_) && layer_ != index) {
            layer_ = index;
            selectedPart_.reset();
            partScroll_ = detailScroll_ = 0;
        }
    }
    auto blood = "Blood "+number(injuries.bloodVolume())+" / "+number(injuries.maximumBloodVolume())+" mL";
    if (injuries.bloodLossRate() > 0) blood += "  |  -"+number(injuries.bloodLossRate(),1)+" mL/s";
    auto condition = "Trauma "+std::to_string(injuries.trauma())+" / "+std::to_string(injuries.lethalThreshold());
    if (!organism.alive()) condition += "  |  Deceased";
    else if (injuries.downed()) condition += "  |  Downed";
    else condition += "  |  Movement "+number(organism.movementMultiplier()*100)+"%  |  Arms "+number(organism.armMultiplier()*100)+"%";
    const float mapWidth = bounds.width*.45F;
    label(canvas,{bounds.x,bounds.y+32,mapWidth-4,15},blood,14);
    label(canvas,{bounds.x+mapWidth+gap,bounds.y+32,bounds.width-mapWidth-gap,15},condition,14,muted);

    const Rect map{bounds.x,bounds.y+54,mapWidth,bounds.height-54};
    const Rect panel{map.x+map.width+gap,map.y,bounds.width-map.width-gap,map.height};
    canvas.fill(map,surface,.05F);
    canvas.fill(panel,surface,.05F);
    canvas.outline(map,border,.05F);
    canvas.outline(panel,border,.05F);

    const auto& layer = customLayers ? options.layers[layer_] : anatomyLayer(injuries.body());
    const auto regions = diagramRegions(map,layer,injuries);
    const auto select = [&](injury::Part part) {
        if (selectedPart_ == part) return;
        selectedPart_ = part;
        detailScroll_ = 0;
    };
    if (regions.empty()) {
        const auto& parts = profile.definition().parts;
        const Rect area{map.x+8,map.y+8,map.width-16,map.height-16};
        constexpr float stride = 33;
        const auto end = visibleEnd(canvas,area,input,parts.size(),stride,partScroll_);
        for (auto index=partScroll_; index<end; ++index) {
            const auto& part = parts[index];
            const Rect row{area.x,area.y+(index-partScroll_)*stride,area.width-7,stride-5};
            if (button(canvas,row,input,part.name,true,selectedPart_ == part.id)) select(part.id);
            drawMarks(canvas,{row.x+row.width*.8F,row.y,row.width*.17F,row.height},part.id,organism,options);
        }
    } else {
        if (input.clicked) {
            const auto hit = std::find_if(regions.rbegin(),regions.rend(),[&](const auto& region) {
                return contains(region.bounds,input);
            });
            if (hit != regions.rend()) select(hit->part);
        }
        for (const auto& region : regions) {
            const bool active = selectedPart_ == region.part;
            const bool hovered = contains(region.bounds,input);
            canvas.fill(region.bounds,active ? selectedFill : hovered ? outline : bodyFill,.45F);
            canvas.outline(region.bounds,active ? accent : border,.45F);
            drawMarks(canvas,region.bounds,region.part,organism,options);
        }
        const auto hovered = std::find_if(regions.rbegin(),regions.rend(),[&](const auto& region) {
            return contains(region.bounds,input);
        });
        if (hovered != regions.rend()) {
            const auto* part = profile.part(hovered->part);
            label(canvas,{map.x+8,map.y+6,map.width-16,14},part->name,12);
        }
    }
    if (selectedPart_ && button(canvas,{panel.x+panel.width-91,panel.y+6,83,24},input,"Overview",true)) {
        selectedPart_.reset();
        detailScroll_ = 0;
    }
    const auto* selected = selectedPart_ ? profile.part(*selectedPart_) : nullptr;
    label(canvas,{panel.x+10,panel.y+9,panel.width-(selected ? 108 : 20),20},selected ? selected->name : "Whole body",17);
    const auto rows = details(organism,selectedPart_,options);
    const Rect area{panel.x+8,panel.y+38,panel.width-16,panel.height-44};
    constexpr float stride = 41;
    const auto end = visibleEnd(canvas,area,input,rows.size(),stride,detailScroll_);
    std::optional<ActionRequest> request;
    for (auto index=detailScroll_; index<end; ++index) {
        const auto& row = rows[index];
        const Rect box{area.x,area.y+(index-detailScroll_)*stride,area.width-7,stride-5};
        float titleWidth=box.width-16;
        if (!row.title.empty()) canvas.fill(box,card,.20F);
        if (row.action) {
            const float actionWidth = std::min(116.F,box.width*.46F);
            const Rect control = row.title.empty() ? box : Rect{box.x+box.width-actionWidth-5,box.y+2,actionWidth,18};
            if (button(canvas,control,input,row.action->label,row.action->enabled)) {
                const auto* effectId = std::get_if<std::string>(&row.subject);
                request=ActionRequest{{row.action->id,*selectedPart_,row.subject},
                    effectId ? std::optional<std::string>{*effectId} : std::nullopt};
            }
            if (row.title.empty()) continue;
            titleWidth-=actionWidth+4;
        }
        label(canvas,{box.x+8,box.y+3,titleWidth,16},row.title,14,row.color);
        label(canvas,{box.x+8,box.y+20,box.width-16,13},row.detail,12);
    }
    return request;
}

} // namespace colony::physiology::viewer
