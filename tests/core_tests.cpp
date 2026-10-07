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
        djmeta::SafetyClass::Review, "ordering test"
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

void test_persisted_ruleset_loader() {
    const auto ruleset = djmeta::parse_ruleset_json(read_text("rules/default-rules.json"));
    require(ruleset.schema_version == 1, "persisted schema version mismatch");
    require(ruleset.id == "lxsdd.dj-metadata-normalizer.default", "persisted ruleset id mismatch");
    require(ruleset.revision == "2026-10-07.1", "persisted ruleset revision mismatch");
    require(ruleset.rules.size() == 2, "persisted default rule count mismatch");
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

    require_invalid_ruleset(
        R"({"schema_version":2,"ruleset_id":"x","revision":"r","rules":[]})",
        "unsupported schema version was accepted");
    require_invalid_ruleset(
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[{"id":"safe.x","enabled":true,"priority":1,"fields":["TITLE"],"match":{"kind":"always"},"transform":{"kind":"replace_with","replacement":"X"},"safety":"SAFE","source":{"kind":"manual","rationale":"unsafe"}}]})",
        "semantic replacement was accepted as SAFE");
    require_invalid_ruleset(
        R"({"schema_version":1,"ruleset_id":"x","revision":"r","rules":[],"unknown":true})",
        "unknown top-level property was accepted");
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
    test_exact_alias_and_safety();
    test_multivalue_preservation();
    test_disabled_rule();
    test_equal_priority_is_stable_by_rule_id();
    test_persisted_ruleset_loader();
    test_fingerprint_contract();
    std::cout << "PASS: djmeta core deterministic preview tests\n";
    return 0;
}
