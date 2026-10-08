#pragma once

// Shared, SDK-free, versioned layouts for the two foobar review grids.
// Native host cfg storage is in batch_table_settings.cpp; this pure model
// deliberately does not own any audio, tag or filesystem operations.
#include <array>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace djmeta {

template<std::size_t N>
struct ReviewGridLayout {
    static_assert(N > 0 && N <= 8);
    std::array<int, N> order{};
    std::array<int, N> widths{};
    unsigned visible_mask = (1u << N) - 1u;
    int sort_column = 0;
    bool sort_descending = false;
};

template<std::size_t N>
ReviewGridLayout<N> default_review_grid_layout(std::array<int, N> widths) {
    ReviewGridLayout<N> result;
    result.widths = widths;
    for (std::size_t i = 0; i < N; ++i) result.order[i] = static_cast<int>(i);
    return result;
}

template<std::size_t N>
bool valid_review_grid_layout(const ReviewGridLayout<N>& layout) {
    if (layout.visible_mask == 0 ||
        layout.visible_mask > (1u << N) - 1u ||
        layout.sort_column < 0 ||
        layout.sort_column >= static_cast<int>(N))
        return false;
    std::array<bool, N> seen{};
    for (std::size_t i = 0; i < N; ++i) {
        const int column = layout.order[i];
        if (column < 0 || column >= static_cast<int>(N) ||
            seen[static_cast<std::size_t>(column)])
            return false;
        seen[static_cast<std::size_t>(column)] = true;
        if (layout.widths[i] < 48 || layout.widths[i] > 3000) return false;
    }
    return true;
}

namespace review_layout_detail {
inline std::vector<std::string_view> split(std::string_view input, char delimiter) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (;;) {
        const auto end = input.find(delimiter, start);
        parts.push_back(input.substr(start, end == std::string_view::npos
                                           ? end : end - start));
        if (end == std::string_view::npos) return parts;
        start = end + 1;
    }
}
inline bool integer(std::string_view input, int& out) {
    if (input.empty()) return false;
    const auto [end, error] = std::from_chars(input.data(),
        input.data() + input.size(), out);
    return error == std::errc{} && end == input.data() + input.size();
}
template<std::size_t N>
bool parse_array(std::string_view input, std::array<int, N>& out) {
    const auto parts = split(input, ',');
    if (parts.size() != N) return false;
    for (std::size_t i = 0; i < N; ++i)
        if (!integer(parts[i], out[i])) return false;
    return true;
}
} // namespace review_layout_detail

template<std::size_t N>
std::string serialize_review_grid_layout(const ReviewGridLayout<N>& layout) {
    if (!valid_review_grid_layout(layout)) return {};
    std::string out = "v1|";
    for (std::size_t i = 0; i < N; ++i) {
        if (i) out += ',';
        out += std::to_string(layout.order[i]);
    }
    out += '|';
    for (std::size_t i = 0; i < N; ++i) {
        if (i) out += ',';
        out += std::to_string(layout.widths[i]);
    }
    out += '|' + std::to_string(layout.visible_mask);
    out += '|' + std::to_string(layout.sort_column);
    out += '|';
    out += layout.sort_descending ? '1' : '0';
    return out;
}

template<std::size_t N>
ReviewGridLayout<N> parse_review_grid_layout(
    std::string_view input, const ReviewGridLayout<N>& defaults) {
    const auto parts = review_layout_detail::split(input, '|');
    if (parts.size() != 6 || parts[0] != "v1") return defaults;
    ReviewGridLayout<N> parsed;
    int mask = 0, descending = 0;
    if (!review_layout_detail::parse_array(parts[1], parsed.order) ||
        !review_layout_detail::parse_array(parts[2], parsed.widths) ||
        !review_layout_detail::integer(parts[3], mask) ||
        !review_layout_detail::integer(parts[4], parsed.sort_column) ||
        !review_layout_detail::integer(parts[5], descending) ||
        mask < 0 || (descending != 0 && descending != 1))
        return defaults;
    parsed.visible_mask = static_cast<unsigned>(mask);
    parsed.sort_descending = descending != 0;
    return valid_review_grid_layout(parsed) ? parsed : defaults;
}

template<std::size_t N>
bool set_review_column_visible(ReviewGridLayout<N>& layout,
                               std::size_t column, bool visible) {
    if (column >= N) return false;
    const auto bit = 1u << column;
    if (!visible && layout.visible_mask == bit) return false;
    if (visible) layout.visible_mask |= bit;
    else layout.visible_mask &= ~bit;
    return true;
}

} // namespace djmeta
