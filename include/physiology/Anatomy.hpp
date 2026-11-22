#pragma once

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

namespace colony::physiology {

struct WholeBody {};

template<class Part> using EffectTarget = std::variant<WholeBody, Part, std::vector<Part>>;

template<class Part> struct PartDefinition {
    Part id;
    std::string_view name;
    double hitUpperBound;
};

// A view of an immutable definition that must outlive the anatomy.
template<class Part> class Anatomy {
public:
    template<std::size_t N>
    constexpr explicit Anatomy(const std::array<PartDefinition<Part>, N>& parts)
        : begin_(parts.data()), end_(parts.data() + N) {
        static_assert(N > 0, "An anatomy must have at least one part");
    }
    template<std::size_t N>
    Anatomy(std::array<PartDefinition<Part>, N>&&) = delete;

    explicit Anatomy(const std::vector<PartDefinition<Part>>& parts)
        : begin_(parts.data()), end_(begin_) {
        if (parts.empty()) throw std::invalid_argument("An anatomy must have at least one part.");
        end_ += parts.size();
    }
    Anatomy(std::vector<PartDefinition<Part>>&&) = delete;

    [[nodiscard]] const PartDefinition<Part>* find(Part id) const {
        const auto part = std::find_if(begin_, end_, [id](const auto& entry) { return entry.id == id; });
        return part == end_ ? nullptr : part;
    }
    [[nodiscard]] Part hitPart(double roll) const {
        // Each upper bound is cumulative, with the last part covering the remainder.
        const auto part = std::find_if(begin_, end_ - 1, [roll](const auto& entry) { return roll < entry.hitUpperBound; });
        return part->id;
    }
    [[nodiscard]] bool supports(const EffectTarget<Part>& target) const {
        if (const auto* part = std::get_if<Part>(&target)) return find(*part) != nullptr;
        if (const auto* parts = std::get_if<std::vector<Part>>(&target)) {
            if (parts->empty()) return false;
            for (auto part = parts->begin(); part != parts->end(); ++part) {
                if (!find(*part) || std::find(parts->begin(), part, *part) != part) return false;
            }
        }
        return true;
    }

private:
    const PartDefinition<Part>* begin_;
    const PartDefinition<Part>* end_;
};

} // namespace colony::physiology
