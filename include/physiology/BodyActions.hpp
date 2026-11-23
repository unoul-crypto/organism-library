#pragma once

#include "physiology/Organism.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace colony::physiology {

struct BodyActionTarget {
    std::string id;
    injury::Part part;

    bool operator==(const BodyActionTarget& other) const { return id == other.id && part == other.part; }
};

struct BodyActionDefinition {
    std::string id;
    std::function<bool(Organism&,injury::Part)> apply;
    // An empty predicate accepts any existing part on a living organism.
    std::function<bool(const Organism&,injury::Part)> eligible;
};

class BodyActionCatalog {
public:
    // IDs must be unique, and each definition must supply an operation.
    explicit BodyActionCatalog(std::vector<BodyActionDefinition> definitions);

    [[nodiscard]] const BodyActionDefinition* find(std::string_view id) const;
    [[nodiscard]] bool canApply(const Organism& organism,std::string_view id,injury::Part part) const;
    [[nodiscard]] bool canApply(const Organism& organism,const BodyActionTarget& target) const {
        return canApply(organism,target.id,target.part);
    }
    // Prepare the resulting state without changing the original. Callers can commit it
    // after a separate operation, such as consuming a required item, succeeds.
    [[nodiscard]] std::optional<Organism> prepare(const Organism& organism,const BodyActionTarget& target) const;
    bool apply(Organism& organism,const BodyActionTarget& target) const;

private:
    std::vector<BodyActionDefinition> definitions_;
};

} // namespace colony::physiology
