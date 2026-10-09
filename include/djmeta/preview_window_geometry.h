#pragma once

// Pure/portable window size persistence schema. Physical Win32 pixels and
// monitor geometry remain a host-layer responsibility. No disk/network/audio.
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace djmeta {

struct PreviewWindowSize {
    int width_at_96_dpi = 0;
    int height_at_96_dpi = 0;
    bool operator==(const PreviewWindowSize&) const = default;
};

inline bool valid_preview_window_size(PreviewWindowSize size) {
    return size.width_at_96_dpi >= 320 && size.width_at_96_dpi <= 8192 &&
           size.height_at_96_dpi >= 240 && size.height_at_96_dpi <= 8192;
}

inline std::optional<PreviewWindowSize> parse_preview_window_size(std::string_view value) {
    if (!value.starts_with("v1|")) return std::nullopt;
    value.remove_prefix(3);
    const auto separator = value.find('|');
    if (separator == std::string_view::npos ||
        separator == 0 || separator == value.size() - 1 ||
        value.find('|', separator + 1) != std::string_view::npos)
        return std::nullopt;
    const auto parse_number = [](std::string_view digits, int& number) {
        const auto result = std::from_chars(digits.data(),
                                           digits.data() + digits.size(), number);
        return result.ec == std::errc{} &&
               result.ptr == digits.data() + digits.size();
    };
    PreviewWindowSize out;
    if (!parse_number(value.substr(0, separator), out.width_at_96_dpi) ||
        !parse_number(value.substr(separator + 1), out.height_at_96_dpi) ||
        !valid_preview_window_size(out))
        return std::nullopt;
    return out;
}

inline std::string serialize_preview_window_size(PreviewWindowSize size) {
    if (!valid_preview_window_size(size)) return {};
    return "v1|" + std::to_string(size.width_at_96_dpi) + "|" +
           std::to_string(size.height_at_96_dpi);
}

} // namespace djmeta
