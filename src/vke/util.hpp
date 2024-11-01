#pragma once

#include <concepts>
#include <ranges>

namespace vke {
    template <std::ranges::contiguous_range Range>
    constexpr std::size_t byte_size(Range &&range) {
        return std::ranges::size(range) * sizeof(std::ranges::range_value_t<Range>);
    };
} // namespace vke
