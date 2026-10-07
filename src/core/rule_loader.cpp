#include "djmeta/rule_loader.h"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace djmeta {
namespace {

class JsonReader {
public:
    explicit JsonReader(std::string_view text) : text_(text) {}

    bool consume(char expected) {
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) fail(std::string("expected '") + expected + "'");
    }

    std::string parse_string() {
        skip_ws();
        if (pos_ >= text_.size() || text_[pos_] != '"') fail("expected string");
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (pos_ >= text_.size()) fail("unterminated escape");
            const char esc = text_[pos_++];
            switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                std::uint32_t cp = parse_hex4();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (pos_ + 2 > text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u')
                        fail("high surrogate without low surrogate");
                    pos_ += 2;
                    const std::uint32_t low = parse_hex4();
                    if (low < 0xDC00 || low > 0xDFFF) fail("invalid low surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail("unexpected low surrogate");
                }
                append_utf8(out, cp);
                break;
            }
            default:
                fail("unsupported escape");
            }
        }
        fail("unterminated string");
    }

    int parse_int() {
        skip_ws();
        if (pos_ >= text_.size()) fail("expected integer");
        const std::size_t begin = pos_;
        if (text_[pos_] == '-') ++pos_;
        const std::size_t digits = pos_;
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        if (digits == pos_) fail("expected integer digits");
        if (pos_ < text_.size() && (text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E'))
            fail("integer field must not be fractional");
        int result = 0;
        const auto* first = text_.data() + begin;
        const auto* last = text_.data() + pos_;
        const auto parsed = std::from_chars(first, last, result);
        if (parsed.ec != std::errc{} || parsed.ptr != last) fail("integer out of range");
        return result;
    }

    bool parse_bool() {
        skip_ws();
        if (text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return true;
        }
        if (text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return false;
        }
        fail("expected boolean");
    }

    std::vector<std::string> parse_string_array() {
        std::vector<std::string> values;
        expect('[');
        if (consume(']')) return values;
        while (true) {
            values.push_back(parse_string());
            if (consume(']')) break;
            expect(',');
        }
        return values;
    }

    void require_end() {
        skip_ws();
        if (pos_ != text_.size()) fail("trailing content");
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::invalid_argument(
            "ruleset JSON at byte " + std::to_string(pos_) + ": " + message);
    }

private:
    void skip_ws() {
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_]);
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++pos_;
        }
    }

    std::uint32_t parse_hex4() {
        if (pos_ + 4 > text_.size()) fail("incomplete unicode escape");
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
            else fail("invalid unicode escape");
        }
        return value;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            throw std::invalid_argument("ruleset JSON: unicode code point out of range");
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

void mark_once(JsonReader& reader, bool& flag, std::string_view key) {
    if (flag) reader.fail("duplicate property: " + std::string(key));
    flag = true;
}

bool valid_rule_id(std::string_view id) {
    if (id.size() < 2) return false;
    const auto first = static_cast<unsigned char>(id.front());
    if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) return false;
    for (unsigned char c : id.substr(1)) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-') continue;
        return false;
    }
    return true;
}

MatchKind parse_match_kind(JsonReader& reader, const std::string& value) {
    if (value == "always") return MatchKind::Always;
    if (value == "exact") return MatchKind::Exact;
    reader.fail("unsupported match kind: " + value);
}

TransformKind parse_transform_kind(JsonReader& reader, const std::string& value) {
    if (value == "trim_whitespace") return TransformKind::TrimWhitespace;
    if (value == "collapse_whitespace") return TransformKind::CollapseWhitespace;
    if (value == "replace_with") return TransformKind::ReplaceWith;
    reader.fail("unsupported transform kind: " + value);
}

SafetyClass parse_safety(JsonReader& reader, const std::string& value) {
    if (value == "SAFE") return SafetyClass::Safe;
    if (value == "CONFIDENT") return SafetyClass::Confident;
    if (value == "REVIEW") return SafetyClass::Review;
    reader.fail("unsupported safety class: " + value);
}

void parse_match(JsonReader& reader, Rule& rule) {
    bool have_kind = false;
    bool have_value = false;
    bool have_case_sensitive = false;
    std::string kind;

    reader.expect('{');
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.parse_string();
            reader.expect(':');
            if (key == "kind") {
                mark_once(reader, have_kind, key);
                kind = reader.parse_string();
            } else if (key == "value") {
                mark_once(reader, have_value, key);
                rule.match_value = reader.parse_string();
            } else if (key == "case_sensitive") {
                mark_once(reader, have_case_sensitive, key);
                rule.case_sensitive = reader.parse_bool();
            } else {
                reader.fail("unknown match property: " + key);
            }
            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }

    if (!have_kind) reader.fail("match.kind is required");
    rule.match = parse_match_kind(reader, kind);
    if (rule.match == MatchKind::Exact && !have_value)
        reader.fail("exact match requires value");
}

void parse_transform(JsonReader& reader, Rule& rule) {
    bool have_kind = false;
    bool have_replacement = false;
    std::string kind;

    reader.expect('{');
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.parse_string();
            reader.expect(':');
            if (key == "kind") {
                mark_once(reader, have_kind, key);
                kind = reader.parse_string();
            } else if (key == "replacement") {
                mark_once(reader, have_replacement, key);
                rule.replacement = reader.parse_string();
            } else {
                reader.fail("unknown transform property: " + key);
            }
            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }

    if (!have_kind) reader.fail("transform.kind is required");
    rule.transform = parse_transform_kind(reader, kind);
    if (rule.transform == TransformKind::ReplaceWith && !have_replacement)
        reader.fail("replace_with requires replacement");
}

void parse_source(JsonReader& reader, Rule& rule) {
    bool have_kind = false;
    bool have_rationale = false;
    bool have_reference = false;

    reader.expect('{');
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.parse_string();
            reader.expect(':');
            if (key == "kind") {
                mark_once(reader, have_kind, key);
                rule.source_kind = reader.parse_string();
            } else if (key == "rationale") {
                mark_once(reader, have_rationale, key);
                rule.rationale = reader.parse_string();
            } else if (key == "reference") {
                mark_once(reader, have_reference, key);
                rule.source_reference = reader.parse_string();
            } else {
                reader.fail("unknown source property: " + key);
            }
            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }

    if (!have_kind || !have_rationale) reader.fail("source.kind and source.rationale are required");
    if (rule.rationale.empty()) reader.fail("source.rationale must not be empty");
    if (rule.source_kind != "builtin" &&
        rule.source_kind != "migrated_masstagger" &&
        rule.source_kind != "user_correction" &&
        rule.source_kind != "manual")
        reader.fail("unsupported source kind: " + rule.source_kind);
}

Rule parse_rule(JsonReader& reader) {
    Rule rule;
    bool have_id = false;
    bool have_enabled = false;
    bool have_priority = false;
    bool have_fields = false;
    bool have_match = false;
    bool have_transform = false;
    bool have_safety = false;
    bool have_source = false;

    reader.expect('{');
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.parse_string();
            reader.expect(':');
            if (key == "id") {
                mark_once(reader, have_id, key);
                rule.id = reader.parse_string();
            } else if (key == "enabled") {
                mark_once(reader, have_enabled, key);
                rule.enabled = reader.parse_bool();
            } else if (key == "priority") {
                mark_once(reader, have_priority, key);
                rule.priority = reader.parse_int();
            } else if (key == "fields") {
                mark_once(reader, have_fields, key);
                rule.fields = reader.parse_string_array();
            } else if (key == "match") {
                mark_once(reader, have_match, key);
                parse_match(reader, rule);
            } else if (key == "transform") {
                mark_once(reader, have_transform, key);
                parse_transform(reader, rule);
            } else if (key == "safety") {
                mark_once(reader, have_safety, key);
                rule.safety = parse_safety(reader, reader.parse_string());
            } else if (key == "source") {
                mark_once(reader, have_source, key);
                parse_source(reader, rule);
            } else {
                reader.fail("unknown rule property: " + key);
            }
            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }

    if (!have_id || !have_enabled || !have_priority || !have_fields ||
        !have_match || !have_transform || !have_safety || !have_source)
        reader.fail("rule is missing a required property");
    if (!valid_rule_id(rule.id)) reader.fail("invalid stable rule id: " + rule.id);
    if (rule.fields.empty()) reader.fail("rule.fields must not be empty");
    for (const std::string& field : rule.fields)
        if (field.empty()) reader.fail("rule.fields contains an empty field name");

    if (rule.safety == SafetyClass::Safe &&
        rule.transform != TransformKind::TrimWhitespace &&
        rule.transform != TransformKind::CollapseWhitespace)
        reader.fail("SAFE rule uses an unqualified semantic transform: " + rule.id);

    return rule;
}

std::vector<Rule> parse_rules(JsonReader& reader) {
    std::vector<Rule> rules;
    reader.expect('[');
    if (reader.consume(']')) return rules;
    while (true) {
        rules.push_back(parse_rule(reader));
        if (reader.consume(']')) break;
        reader.expect(',');
    }
    return rules;
}

} // namespace

Ruleset parse_ruleset_json(std::string_view json) {
    JsonReader reader(json);
    Ruleset out;
    bool have_schema = false;
    bool have_id = false;
    bool have_revision = false;
    bool have_rules = false;

    reader.expect('{');
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.parse_string();
            reader.expect(':');
            if (key == "schema_version") {
                mark_once(reader, have_schema, key);
                out.schema_version = reader.parse_int();
            } else if (key == "ruleset_id") {
                mark_once(reader, have_id, key);
                out.id = reader.parse_string();
            } else if (key == "revision") {
                mark_once(reader, have_revision, key);
                out.revision = reader.parse_string();
            } else if (key == "rules") {
                mark_once(reader, have_rules, key);
                out.rules = parse_rules(reader);
            } else {
                reader.fail("unknown ruleset property: " + key);
            }
            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }
    reader.require_end();

    if (!have_schema || !have_id || !have_revision || !have_rules)
        reader.fail("ruleset is missing a required property");
    if (out.schema_version != 1) reader.fail("unsupported schema_version");
    if (out.id.empty()) reader.fail("ruleset_id must not be empty");
    if (out.revision.empty()) reader.fail("revision must not be empty");

    std::unordered_set<std::string> ids;
    for (const Rule& rule : out.rules) {
        if (!ids.insert(rule.id).second)
            reader.fail("duplicate rule id: " + rule.id);
    }
    return out;
}

MetadataDocument parse_metadata_vectors_json(std::string_view json) {
    JsonReader reader(json);
    MetadataDocument out;

    reader.expect('[');
    if (!reader.consume(']')) {
        while (true) {
            MetadataField field;
            bool have_name = false;
            bool have_values = false;

            reader.expect('{');
            if (!reader.consume('}')) {
                while (true) {
                    const std::string key = reader.parse_string();
                    reader.expect(':');
                    if (key == "name") {
                        mark_once(reader, have_name, key);
                        field.name = reader.parse_string();
                    } else if (key == "values") {
                        mark_once(reader, have_values, key);
                        field.values = reader.parse_string_array();
                    } else {
                        reader.fail("unknown metadata-vector property: " + key);
                    }
                    if (reader.consume('}')) break;
                    reader.expect(',');
                }
            }

            if (!have_name || !have_values)
                reader.fail("metadata vector requires name and values");
            if (field.name.empty())
                reader.fail("metadata vector field name must not be empty");

            out.fields.push_back(std::move(field));
            if (reader.consume(']')) break;
            reader.expect(',');
        }
    }
    reader.require_end();
    return out;
}

} // namespace djmeta
