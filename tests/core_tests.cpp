#include "djmeta/normalizer.h"
#include "djmeta/rule_loader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

djmeta::Rule trim_rule() {
    return djmeta::Rule{
        "safe.trim-whitespace", true, 10, {"*"},
        djmeta::MatchKind::Always, "", false,
        djmeta::TransformKind::TrimWhitespace, "",
        djmeta::SafetyClass::Safe,
        "Remove leading and trailing ASCII whitespace.",
        "builtin", ""
    };
}

djmeta::Rule collapse_rule() {
    return djmeta::Rule{
        "safe.collapse-whitespace", true, 20, {"*"},
        djmeta::MatchKind::Always, "", false,
        djmeta::TransformKind::CollapseWhitespace, "",
        djmeta::SafetyClass::Safe,
        "Collapse repeated ASCII whitespace.",
        "builtin", ""
    };
}

djmeta::Rule unicode_whitespace_rule() {
    return djmeta::Rule{
        "safe.normalize-unicode-whitespace", true, 5, {"*"},
        djmeta::MatchKind::Always, "", false,
        djmeta::TransformKind::NormalizeUnicodeWhitespace, "",
        djmeta::SafetyClass::Safe,
        "Map Unicode White_Space to ASCII space.",
        "builtin", ""
    };
}

void test_preview_is_immutable_and_ordered() {
    const djmeta::MetadataDocument input{{{"TITLE", {"  A   Title  "}}}};
    const djmeta::MetadataDocument input_copy = input;
    const auto result = djmeta::Engine{}.analyze(input, {collapse_rule(), trim_rule()}, "rules-v1-test");

    require(input == input_copy, "analysis mutated the input document");
    require(result.ruleset_revision == "rules-v1-test", "ruleset revision was not retained");
    require(result.changes.size() == 2, "expected trim + collapse trace");
    require(result.changes[0].rule_id == "safe.trim-whitespace", "priority order is not deterministic");
    require(result.changes[0].original_value == "  A   Title  ", "original value not retained");
    require(result.changes[0].before_value == "  A   Title  ", "first trace before-value mismatch");
    require(result.changes[0].proposed_value == "A   Title", "trim result mismatch");
    require(result.changes[1].original_value == "  A   Title  ", "raw original must survive chained rules");
    require(result.changes[1].before_value == "A   Title", "second trace before-value mismatch");
    require(result.changes[1].proposed_value == "A Title", "collapse result mismatch");
    require(result.canonical_preview.fields[0].values[0] == "A Title", "canonical preview mismatch");
    require(result.proposals.size() == 1, "chained rule trace should aggregate to one preview proposal");
    require(result.proposals[0].field_index == 0 && result.proposals[0].value_index == 0, "proposal identity mismatch");
    require(result.proposals[0].original_value == "  A   Title  ", "proposal original mismatch");
    require(result.proposals[0].proposed_value == "A Title", "proposal canonical value mismatch");
    require(result.proposals[0].rule_ids.size() == 2, "proposal rule provenance lost");
    require(result.proposals[0].safety == djmeta::SafetyClass::Safe, "proposal safety aggregation mismatch");
}

void test_unicode_whitespace_schema_v2() {
    const std::string value =
        std::string("\xC2\xA0") + "A" +
        std::string("\xE2\x80\xAF") + "  B" +
        std::string("\xE3\x80\x80");
    const djmeta::MetadataDocument input{{{"TITLE", {value}}}};
    const auto result = djmeta::Engine{}.analyze(
        input,
        {collapse_rule(), trim_rule(), unicode_whitespace_rule()},
        "rules-v2-unicode");

    require(result.changes.size() == 3, "unicode whitespace chain should normalize, trim, and collapse");
    require(result.changes[0].rule_id == "safe.normalize-unicode-whitespace",
            "unicode whitespace rule priority mismatch");
    require(result.changes[1].rule_id == "safe.trim-whitespace",
            "trim should run after unicode whitespace normalization");
    require(result.changes[2].rule_id == "safe.collapse-whitespace",
            "collapse should run after trim");
    require(result.canonical_preview.fields[0].values[0] == "A B",
            "unicode whitespace canonical preview mismatch");
    require(result.proposals.size() == 1, "unicode whitespace chain should aggregate to one proposal");
    require(result.proposals[0].safety == djmeta::SafetyClass::Safe,
            "unicode whitespace proposal must remain SAFE");

    const std::string malformed = std::string("A") + char(0xC2) + "B";
    const djmeta::MetadataDocument malformed_input{{{"TITLE", {malformed}}}};
    const auto malformed_result = djmeta::Engine{}.analyze(
        malformed_input, {unicode_whitespace_rule()}, "malformed");
    require(malformed_result.changes.empty(),
            "malformed UTF-8 bytes must be preserved rather than rewritten");
    require(malformed_result.canonical_preview == malformed_input,
            "malformed UTF-8 preservation changed metadata");
}

void test_exact_alias_and_safety() {
    djmeta::Rule alias{
        "review.label.defected-records", true, 100, {"LABEL"},
        djmeta::MatchKind::Exact, "defected records", false,
        djmeta::TransformKind::ReplaceWith, "Defected",
        djmeta::SafetyClass::Review,
        "Example only; collection aliases must be migrated from qualified user rules.",
        "manual", ""
    };
    const djmeta::MetadataDocument input{{{"label", {"Defected Records"}}}};
    const auto result = djmeta::Engine{}.analyze(input, {alias}, "r2");
    require(result.changes.size() == 1, "case-insensitive exact alias did not match");
    require(result.changes[0].safety == djmeta::SafetyClass::Review, "safety classification changed");
    require(result.canonical_preview.fields[0].values[0] == "Defected", "alias replacement mismatch");
}

void test_multivalue_preservation() {
    const djmeta::MetadataDocument input{{{"ARTIST", {"  Artist A  ", "Artist B"}}}};
    const auto result = djmeta::Engine{}.analyze(input, {trim_rule()}, "r3");
    require(result.changes.size() == 1, "only one multivalue element should change");
    require(result.changes[0].value_index == 0, "multivalue index lost");
    require(result.canonical_preview.fields[0].values.size() == 2, "multivalue cardinality changed");
    require(result.canonical_preview.fields[0].values[0] == "Artist A", "first multivalue not normalized");
    require(result.canonical_preview.fields[0].values[1] == "Artist B", "unchanged multivalue was modified");
}

void test_duplicate_field_names_keep_structural_identity() {
    const djmeta::MetadataDocument input{{
        {"CUSTOM", {"  first  "}},
        {"CUSTOM", {"  second  "}}
    }};
    const auto result = djmeta::Engine{}.analyze(input, {trim_rule()}, "dup-fields");
    require(result.changes.size() == 2, "duplicate fields should both retain independent trace");
    require(result.proposals.size() == 2, "duplicate fields should produce independent proposals");
    require(result.changes[0].field_index == 0 && result.changes[1].field_index == 1,
            "duplicate field trace lost field index");
    require(result.proposals[0].field_index == 0 && result.proposals[1].field_index == 1,
            "duplicate field proposals lost field index");
    require(result.proposals[0].proposed_value == "first", "first duplicate field proposal mismatch");
    require(result.proposals[1].proposed_value == "second", "second duplicate field proposal mismatch");
}

void test_disabled_rule() {
    auto rule = trim_rule();
    rule.enabled = false;
    const djmeta::MetadataDocument input{{{"TITLE", {"  X  "}}}};
    const auto result = djmeta::Engine{}.analyze(input, {rule}, "r4");
    require(result.changes.empty(), "disabled rule executed");
    require(result.canonical_preview == input, "disabled rule changed preview");
}

void test_equal_priority_is_stable_by_rule_id() {
    djmeta::Rule z{
        "z-rule", true, 50, {"TITLE"},
        djmeta::MatchKind::Exact, "Y", true,
        djmeta::TransformKind::ReplaceWith, "Z",
        djmeta::SafetyClass::Review, "ordering test", "manual", ""
    };
    djmeta::Rule a{
        "a-rule", true, 50, {"TITLE"},
        djmeta::MatchKind::Exact, "X", true,
        djmeta::TransformKind::ReplaceWith, "Y",
        djmeta::SafetyClass::Review, "ordering test", "manual", ""
    };

    const djmeta::MetadataDocument input{{{"TITLE", {"X"}}}};
    const auto result = djmeta::Engine{}.analyze(input, {z, a}, "r5");
    require(result.changes.size() == 2, "equal-priority rules should both execute in id order");
    require(result.changes[0].rule_id == "a-rule", "equal-priority rules must sort by stable rule id");
    require(result.changes[1].rule_id == "z-rule", "second equal-priority rule order mismatch");
    require(result.canonical_preview.fields[0].values[0] == "Z", "equal-priority chain result mismatch");
}

std::string read_text(const char* path) {
    std::ifstream in(path, std::ios::binary);
    require(static_cast<bool>(in), "could not open test ruleset");
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void require_invalid_ruleset(const std::string& json, const char* message) {
    try {
        (void)djmeta::parse_ruleset_json(json);
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, message);
}

void test_bridge_metadata_vector_loader() {
    const std::string json =
        R"([{"name":"ARTIST","values":["A","B"]},{"name":"ARTIST","values":["A; B"]},)"
        R"({"name":"EMPTY","values":[""]},{"name":"TITLE","values":["caf\u00e9"]}])";
    const auto doc = djmeta::parse_metadata_vectors_json(json);
    require(doc.fields.size() == 4, "metadata vector field count mismatch");
    require(doc.fields[0].name == "ARTIST" && doc.fields[0].values.size() == 2,
            "true multivalue metadata was not preserved");
    require(doc.fields[1].name == "ARTIST" && doc.fields[1].values[0] == "A; B",
            "duplicate field entry or literal separator was altered");
    require(doc.fields[2].values.size() == 1 && doc.fields[2].values[0].empty(),
            "empty metadata value was not preserved");
    require(doc.fields[3].values[0] == "caf\xC3\xA9", "metadata unicode escape decode mismatch");

    require_invalid_ruleset(
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[],"unknown":true})",
        "unknown top-level property was accepted");

    try {
        (void)djmeta::parse_metadata_vectors_json(R"([{"name":"TITLE","values":[],"unknown":1}])");
        require(false, "unknown metadata-vector property was accepted");
    } catch (const std::invalid_argument&) {
    }
}

void test_persisted_ruleset_loader() {
    const auto ruleset = djmeta::parse_ruleset_json(read_text("rules/default-rules.json"));
    require(ruleset.schema_version == 2, "persisted schema version mismatch");
    require(ruleset.id == "lxsdd.dj-metadata-normalizer.default", "persisted ruleset id mismatch");
    require(ruleset.revision == "2026-10-07.2", "persisted ruleset revision mismatch");
    require(ruleset.rules.size() == 3, "persisted default rule count mismatch");
    require(ruleset.rules[0].source_kind == "builtin", "rule provenance was not loaded");

    const djmeta::MetadataDocument input{{{"TITLE", {"  A   B  "}}}};
    const auto result = djmeta::Engine{}.analyze(input, ruleset.rules, ruleset.revision);
    require(result.changes.size() == 2, "persisted default rules did not execute");
    require(result.canonical_preview.fields[0].values[0] == "A B", "persisted rules canonical preview mismatch");

    const std::string unicode =
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[)"
        R"({"id":"review.x","enabled":true,"priority":1,"fields":["TITLE"],)"
        R"("match":{"kind":"exact","value":"caf\u00e9"},"transform":{"kind":"replace_with","replacement":"Cafe"},)"
        R"("safety":"REVIEW","source":{"kind":"manual","rationale":"caf\u00e9"}}]})";
    const auto parsed_unicode = djmeta::parse_ruleset_json(unicode);
    require(parsed_unicode.rules[0].match_value == "caf\xC3\xA9", "unicode escape did not decode as UTF-8");
    require(parsed_unicode.rules[0].rationale == "caf\xC3\xA9", "unicode provenance did not decode as UTF-8");

    const auto legacy_v1 = djmeta::parse_ruleset_json(
        R"({"schema_version":1,"ruleset_id":"legacy","revision":"r1","rules":[)"
        R"({"id":"safe.trim","enabled":true,"priority":1,"fields":["*"],"match":{"kind":"always"},)"
        R"("transform":{"kind":"trim_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"legacy trim"}}]})");
    require(legacy_v1.schema_version == 1 && legacy_v1.rules.size() == 1,
            "schema-v1 backward compatibility was lost");

    require_invalid_ruleset(
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[)"
        R"({"id":"safe.unicode","enabled":true,"priority":1,"fields":["*"],"match":{"kind":"always"},)"
        R"("transform":{"kind":"normalize_unicode_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"v2 only"}}]})",
        "schema-v1 accepted a schema-v2-only transform");

    require_invalid_ruleset(
        R"({"schema_version":3,"ruleset_id":"x","revision":"r","rules":[]})",
        "unsupported schema version was accepted");
    require_invalid_ruleset(
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[{"id":"safe.x","enabled":true,"priority":1,"fields":["TITLE"],"match":{"kind":"always"},"transform":{"kind":"replace_with","replacement":"X"},"safety":"SAFE","source":{"kind":"manual","rationale":"unsafe"}}]})",
        "semantic replacement was accepted as SAFE");
}

void test_fingerprint_contract() {
    const djmeta::MetadataDocument a{{{"ARTIST", {"A", "B"}}, {"TITLE", {"Track"}}}};
    const djmeta::MetadataDocument b = a;
    djmeta::MetadataDocument c = a;
    c.fields[0].values[1] = "C";

    const std::string fa = djmeta::fingerprint(a);
    require(fa.size() == 64, "fingerprint is not SHA-256 hex width");
    require(fa == djmeta::fingerprint(b), "equal documents must fingerprint equally");
    require(fa != djmeta::fingerprint(c), "metadata change must change fingerprint");
    const std::string empty = djmeta::fingerprint({});
    require(empty == "bc90e3740621acaeaf320fd210e1e592dcdb12f0f3e7e67a89c0b5e92bb3c8ad",
            "empty document SHA-256 regression vector changed");
}

} // namespace

int main() {
    test_preview_is_immutable_and_ordered();
    test_unicode_whitespace_schema_v2();
    test_exact_alias_and_safety();
    test_multivalue_preservation();
    test_duplicate_field_names_keep_structural_identity();
    test_disabled_rule();
    test_equal_priority_is_stable_by_rule_id();
    test_bridge_metadata_vector_loader();
    test_persisted_ruleset_loader();
    test_fingerprint_contract();
    std::cout << "PASS: djmeta core deterministic preview tests\n";
    return 0;
}
