#pragma once

// Shared structural metadata protection for normalization, SAFE staging and
// explicit review projection. A generic metadata pipeline must never rewrite
// embedded CUE records, even when a malformed/forged proposal is supplied.
#include <string_view>
#include <cstdint>

namespace djmeta {

// A malformed legacy original may remain byte-identical, but any NEW
// normalized, accepted or manual tag text must be well-formed UTF-8.
inline bool valid_utf8_metadata_text(std::string_view value) {
    for (std::size_t pos = 0; pos < value.size();) {
        const unsigned char first = static_cast<unsigned char>(value[pos]);
        if (first < 0x80u) { ++pos; continue; }
        std::uint32_t cp = 0, minimum = 0;
        std::size_t width = 0;
        if ((first & 0xe0u) == 0xc0u) {
            width = 2; cp = first & 0x1fu; minimum = 0x80u;
        } else if ((first & 0xf0u) == 0xe0u) {
            width = 3; cp = first & 0x0fu; minimum = 0x800u;
        } else if ((first & 0xf8u) == 0xf0u) {
            width = 4; cp = first & 0x07u; minimum = 0x10000u;
        } else return false;
        if (width > value.size() - pos) return false;
        for (std::size_t i = 1; i < width; ++i) {
            const unsigned char next = static_cast<unsigned char>(value[pos + i]);
            if ((next & 0xc0u) != 0x80u) return false;
            cp = (cp << 6) | (next & 0x3fu);
        }
        if (cp < minimum || cp > 0x10ffffu ||
            (cp >= 0xd800u && cp <= 0xdfffu)) return false;
        pos += width;
    }
    return true;
}

inline bool metadata_field_equals_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char ch) {
            return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
        };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

inline bool is_protected_cue_metadata(std::string_view name) {
    return metadata_field_equals_ci(name, "CUESHEET") ||
           metadata_field_equals_ci(name, "CUE_SHEET") ||
           metadata_field_equals_ci(name, "__CUESHEET");
}

inline bool is_lyrics_metadata(std::string_view name) {
    return metadata_field_equals_ci(name, "LYRICS") ||
           metadata_field_equals_ci(name, "UNSYNCEDLYRICS") ||
           metadata_field_equals_ci(name, "SYNCEDLYRICS") ||
           metadata_field_equals_ci(name, "UNSYNCED LYRICS") ||
           metadata_field_equals_ci(name, "USLT") ||
           metadata_field_equals_ci(name, "SYLT");
}

inline bool has_structured_line_break(std::string_view value) {
    return value.find_first_of("\r\n") != std::string_view::npos ||
           value.find("\xC2\x85") != std::string_view::npos ||
           value.find("\xE2\x80\xA8") != std::string_view::npos ||
           value.find("\xE2\x80\xA9") != std::string_view::npos;
}

// AUTO-SAFE should only apply to scalar, single-line content.
// Explicit REVIEW editing is still allowed for lyrics and comments.
inline bool safe_scalar_metadata_field(std::string_view name, std::string_view original) {
    return !is_protected_cue_metadata(name) &&
           !is_lyrics_metadata(name) && !has_structured_line_break(original) &&
           original.find('\0') == std::string_view::npos;
}

} // namespace djmeta
