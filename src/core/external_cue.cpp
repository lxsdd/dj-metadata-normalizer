#include "djmeta/external_cue.h"

#include <cstdint>
#include <string_view>

namespace djmeta {
namespace {

bool space(unsigned char ch) { return ch == ' ' || ch == '\t'; }
char upper_ascii(char ch) {
    return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch;
}

bool equal_ascii_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (upper_ascii(a[i]) != upper_ascii(b[i])) return false;
    return true;
}

bool valid_utf8(std::string_view text) {
    for (std::size_t pos = 0; pos < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[pos]);
        if (first < 0x80u) { ++pos; continue; }
        std::size_t count = 0;
        std::uint32_t cp = 0, minimum = 0;
        if ((first & 0xe0u) == 0xc0u) {
            count = 2; cp = first & 0x1fu; minimum = 0x80u;
        } else if ((first & 0xf0u) == 0xe0u) {
            count = 3; cp = first & 0x0fu; minimum = 0x800u;
        } else if ((first & 0xf8u) == 0xf0u) {
            count = 4; cp = first & 0x07u; minimum = 0x10000u;
        } else return false;
        if (count > text.size() - pos) return false;
        for (std::size_t i = 1; i < count; ++i) {
            const unsigned char next = static_cast<unsigned char>(text[pos + i]);
            if ((next & 0xc0u) != 0x80u) return false;
            cp = (cp << 6) | (next & 0x3fu);
        }
        if (cp < minimum || cp > 0x10ffffu ||
            (cp >= 0xd800u && cp <= 0xdfffu)) return false;
        pos += count;
    }
    return true;
}

bool is_known_file_type(std::string_view token) {
    return equal_ascii_ci(token, "WAVE") || equal_ascii_ci(token, "MP3") ||
           equal_ascii_ci(token, "AIFF") || equal_ascii_ci(token, "BINARY") ||
           equal_ascii_ci(token, "MOTOROLA");
}

bool is_absolute_file_path(std::string_view path) {
    if (path.empty()) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
    if (path.size() >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z'))) return true;
    return false;
}

bool has_control_byte(std::string_view value) {
    for (const unsigned char ch : value)
        if (ch < 0x20u || ch == 0x7fu) return true;
    return false;
}

void parse_file_line(
    ExternalCueInventory& out,
    std::string_view line,
    std::size_t line_offset,
    std::size_t line_number,
    std::size_t pos) {

    auto skip_spaces = [&]() {
        while (pos < line.size() && space(static_cast<unsigned char>(line[pos]))) ++pos;
    };
    skip_spaces();
    if (pos == line.size()) {
        out.status = CueSyntaxStatus::Invalid;
        out.diagnostics.emplace_back("FILE_PATH_MISSING");
        return;
    }

    ExternalCueReference ref;
    ref.line_number = line_number;
    ref.quoted = line[pos] == '"';
    std::size_t begin = 0, end = 0;
    if (ref.quoted) {
        begin = ++pos;
        while (pos < line.size() && line[pos] != '"') ++pos;
        if (pos == line.size()) {
            out.status = CueSyntaxStatus::Invalid;
            out.diagnostics.emplace_back("FILE_QUOTE_UNTERMINATED");
            return;
        }
        end = pos;
        ++pos;
        if (pos < line.size() && !space(static_cast<unsigned char>(line[pos]))) {
            out.status = CueSyntaxStatus::Invalid;
            out.diagnostics.emplace_back("FILE_QUOTE_NOT_DELIMITED");
            return;
        }
    } else {
        begin = pos;
        while (pos < line.size() && !space(static_cast<unsigned char>(line[pos]))) ++pos;
        end = pos;
    }
    ref.filename_begin = line_offset + begin;
    ref.filename_end = line_offset + end;
    ref.filename = std::string(line.substr(begin, end - begin));
    if (ref.filename.empty() || has_control_byte(ref.filename)) {
        out.status = CueSyntaxStatus::Invalid;
        out.diagnostics.emplace_back("FILE_NAME_INVALID");
        return;
    }

    skip_spaces();
    const std::size_t type_begin = pos;
    while (pos < line.size() && !space(static_cast<unsigned char>(line[pos]))) ++pos;
    if (type_begin == pos) {
        out.status = CueSyntaxStatus::Invalid;
        out.diagnostics.emplace_back("FILE_TYPE_MISSING");
        return;
    }
    ref.file_type = std::string(line.substr(type_begin, pos - type_begin));
    skip_spaces();
    if (pos != line.size()) {
        out.status = CueSyntaxStatus::Invalid;
        out.diagnostics.emplace_back("FILE_TRAILING_DATA");
        return;
    }

    if (!is_known_file_type(ref.file_type)) {
        if (out.status != CueSyntaxStatus::Invalid)
            out.status = CueSyntaxStatus::NeedsReview;
        out.diagnostics.emplace_back("FILE_TYPE_UNRECOGNIZED");
    }
    if (is_absolute_file_path(ref.filename)) {
        if (out.status != CueSyntaxStatus::Invalid)
            out.status = CueSyntaxStatus::NeedsReview;
        out.diagnostics.emplace_back("ABSOLUTE_FILE_REFERENCE_REQUIRES_HOST_RESOLUTION");
    }
    out.references.emplace_back(std::move(ref));
}

} // namespace

ExternalCueInventory inspect_external_cue(std::string_view raw_bytes) {
    ExternalCueInventory out;
    out.source_bytes = raw_bytes.size();
    out.status = CueSyntaxStatus::Parsed;
    out.encoding = CueTextEncoding::Utf8;

    // Refuse to parse unknown or non-UTF-8 text as if it were UTF-8.
    // Host adapter may later provide a separately qualified charset decoder.
    if (raw_bytes.size() > 8u * 1024u * 1024u) {
        out.status = CueSyntaxStatus::NeedsReview;
        out.diagnostics.emplace_back("CUE_TOO_LARGE_FOR_SYNTAX_INVENTORY");
        return out;
    }
    if (raw_bytes.size() >= 2 &&
        ((static_cast<unsigned char>(raw_bytes[0]) == 0xffu &&
          static_cast<unsigned char>(raw_bytes[1]) == 0xfeu) ||
         (static_cast<unsigned char>(raw_bytes[0]) == 0xfeu &&
          static_cast<unsigned char>(raw_bytes[1]) == 0xffu))) {
        out.encoding = CueTextEncoding::UnsupportedUtf16;
        out.status = CueSyntaxStatus::NeedsReview;
        out.diagnostics.emplace_back("CUE_UTF16_REQUIRES_QUALIFIED_DECODER");
        return out;
    }

    std::size_t start = 0;
    if (raw_bytes.size() >= 3 &&
        static_cast<unsigned char>(raw_bytes[0]) == 0xefu &&
        static_cast<unsigned char>(raw_bytes[1]) == 0xbbu &&
        static_cast<unsigned char>(raw_bytes[2]) == 0xbfu) {
        start = 3;
        out.encoding = CueTextEncoding::Utf8Bom;
    }
    if (!valid_utf8(raw_bytes.substr(start)) || raw_bytes.find('\0', start) != std::string_view::npos) {
        out.encoding = CueTextEncoding::Unknown;
        out.status = CueSyntaxStatus::NeedsReview;
        out.diagnostics.emplace_back("CUE_ENCODING_UNQUALIFIED");
        return out;
    }

    std::size_t line_number = 0;
    while (start < raw_bytes.size()) {
        ++line_number;
        const std::size_t line_end = raw_bytes.find_first_of("\r\n", start);
        const std::size_t end = line_end == std::string_view::npos ? raw_bytes.size() : line_end;
        const std::string_view line = raw_bytes.substr(start, end - start);
        std::size_t pos = 0;
        while (pos < line.size() && space(static_cast<unsigned char>(line[pos]))) ++pos;
        const std::size_t verb_begin = pos;
        while (pos < line.size() && !space(static_cast<unsigned char>(line[pos]))) ++pos;
        if (equal_ascii_ci(line.substr(verb_begin, pos - verb_begin), "FILE"))
            parse_file_line(out, line, start, line_number, pos);
        if (line_end == std::string_view::npos) break;
        start = line_end + 1;
        if (raw_bytes[line_end] == '\r' && start < raw_bytes.size() && raw_bytes[start] == '\n')
            ++start;
    }
    out.multiple_files = out.references.size() > 1;
    if (out.references.empty()) {
        out.status = CueSyntaxStatus::Invalid;
        out.diagnostics.emplace_back("CUE_NO_VALID_FILE_REFERENCE");
    }
    return out;
}

} // namespace djmeta
