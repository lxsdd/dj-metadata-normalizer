#pragma once

// Shared structural metadata protection for normalization, SAFE staging and
// explicit review projection. A generic metadata pipeline must never rewrite
// embedded CUE records, even when a malformed/forged proposal is supplied.
#include <string_view>

namespace djmeta {

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
           !is_lyrics_metadata(name) && !has_structured_line_break(original);
}

} // namespace djmeta
