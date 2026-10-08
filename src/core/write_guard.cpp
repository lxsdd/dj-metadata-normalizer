#include "djmeta/write_guard.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace djmeta {
namespace {

char fold_ascii(char ch) {
    return ch >= 'A' && ch <= 'Z'
        ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool normalized_fields(
    const MetadataDocument& doc,
    std::map<std::string, std::vector<std::string>>& out) {
    for (const MetadataField& field : doc.fields) {
        if (field.name.empty() || field.values.empty())
            return false;
        std::string folded;
        folded.reserve(field.name.size());
        for (char ch : field.name) {
            if (ch == '\0') return false;
            folded.push_back(fold_ascii(ch));
        }
        for (const auto& value : field.values)
            if (value.find('\0') != std::string::npos)
                return false;
        if (!out.emplace(std::move(folded), field.values).second)
            return false; // ambiguous duplicate case-insensitive tag key
    }
    return true;
}

} // namespace

TagWriteDecision compare_physical_text_tags(
    const MetadataDocument& original,
    const MetadataDocument& approved_postimage) {

    std::map<std::string, std::vector<std::string>> source, target;
    if (!normalized_fields(original, source) ||
        !normalized_fields(approved_postimage, target))
        return TagWriteDecision::Unqualified;
    return source == target
        ? TagWriteDecision::Unchanged : TagWriteDecision::Changed;
}

bool cue_bytes_rewrite_needed(std::string_view observed_source_bytes,
                              std::string_view approved_postimage_bytes) {
    return observed_source_bytes != approved_postimage_bytes;
}

} // namespace djmeta
