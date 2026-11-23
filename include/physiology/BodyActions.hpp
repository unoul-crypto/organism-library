#pragma once

#include "physiology/Organism.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace colony::physiology {

using BodyActionSubject = std::variant<std::monostate,injury::WoundId,std::string>;

struct BodyActionTarget {
    std::string id;
    injury::Part part;
    BodyActionSubject subject{};

    bool operator==(const BodyActionTarget& other) const {
        return id == other.id && part == other.part && subject == other.subject;
    }
};

template<class Subject>
struct BasicBodyActionDefinition {
    std::string id;
    std::function<bool(Organism&,Subject)> apply;
    // An empty predicate accepts any matching subject on a living organism.
    std::function<bool(const Organism&,Subject)> eligible;
};

using BodyActionDefinition = BasicBodyActionDefinition<injury::Part>;
using WoundActionDefinition = BasicBodyActionDefinition<const injury::Wound&>;
using EffectActionDefinition = BasicBodyActionDefinition<const ActiveEffect&>;
using BodyActionRule = std::variant<BodyActionDefinition,WoundActionDefinition,EffectActionDefinition>;

inline constexpr char bandageWoundActionId[] = "bandage_wound";
[[nodiscard]] WoundActionDefinition bandageWoundAction();

class BodyActionCatalog {
public:
    // IDs must be unique, and each definition must supply an operation.
    explicit BodyActionCatalog(std::vector<BodyActionDefinition> definitions,
        std::vector<WoundActionDefinition> woundActions = {}, std::vector<EffectActionDefinition> effectActions = {});

    [[nodiscard]] const BodyActionDefinition* find(std::string_view id) const;
    [[nodiscard]] const WoundActionDefinition* findWoundAction(std::string_view id) const;
    [[nodiscard]] const EffectActionDefinition* findEffectAction(std::string_view id) const;
    [[nodiscard]] bool recognizes(const BodyActionTarget& target) const;
    [[nodiscard]] bool canApply(const Organism& organism,std::string_view id,injury::Part part) const;
    [[nodiscard]] bool canApply(const Organism& organism,const BodyActionTarget& target) const;
    // Prepare the resulting state without changing the original. Callers can commit it
    // after a separate operation, such as consuming a required item, succeeds.
    [[nodiscard]] std::optional<Organism> prepare(const Organism& organism,const BodyActionTarget& target) const;
    bool apply(Organism& organism,const BodyActionTarget& target) const;

private:
    std::vector<BodyActionDefinition> definitions_;
    std::vector<WoundActionDefinition> woundActions_;
    std::vector<EffectActionDefinition> effectActions_;
};

} // namespace colony::physiology
