#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace djmeta {

enum class SafetyClass { Safe, Confident, Review };
enum class MatchKind { Always, Exact };
enum class TransformKind { NormalizeUnicodeWhitespace, TrimWhitespace, CollapseWhitespace, ReplaceWith };

struct MetadataField {
    std::string name;
    std::vector<std::string> values;
    bool operator==(const MetadataField&) const = default;
};

struct MetadataDocument {
    std::vector<MetadataField> fields;
    bool operator==(const MetadataDocument&) const = default;
};

struct Rule {
    std::string id;
    bool enabled = true;
    int priority = 0;
    std::vector<std::string> fields;
    MatchKind match = MatchKind::Always;
    std::string match_value;
    bool case_sensitive = false;
    TransformKind transform = TransformKind::TrimWhitespace;
    std::string replacement;
    SafetyClass safety = SafetyClass::Review;
    std::string rationale;
    std::string source_kind;
    std::string source_reference;
};

struct Change {
    std::size_t field_index = 0;
    std::string field;
    std::size_t value_index = 0;
    std::string original_value;
    std::string before_value;
    std::string proposed_value;
    std::string rule_id;
    SafetyClass safety = SafetyClass::Review;
    std::string rationale;
    bool operator==(const Change&) const = default;
};

struct Proposal {
    std::size_t field_index = 0;
    std::string field;
    std::size_t value_index = 0;
    std::string original_value;
    std::string proposed_value;
    SafetyClass safety = SafetyClass::Review;
    std::vector<std::string> rule_ids;
    std::vector<std::string> rationales;

    bool operator==(const Proposal&) const = default;
};

struct AnalysisResult {
    std::string input_fingerprint;
    std::string ruleset_revision;
    std::vector<Change> changes;
    std::vector<Proposal> proposals;
    MetadataDocument canonical_preview;
};

class Engine {
public:
    AnalysisResult analyze(
        const MetadataDocument& input,
        const std::vector<Rule>& rules,
        std::string ruleset_revision) const;
};

std::string fingerprint(const MetadataDocument& input);
const char* to_string(SafetyClass value);

} // namespace djmeta
