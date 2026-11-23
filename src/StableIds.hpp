#pragma once

#include <algorithm>
#include <string_view>

namespace colony::physiology::detail {

inline bool validId(std::string_view id) {
    return !id.empty() && std::all_of(id.begin(),id.end(),[](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}

} // namespace colony::physiology::detail
