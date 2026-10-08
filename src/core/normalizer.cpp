#include "djmeta/normalizer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace djmeta {
namespace {

bool ascii_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool unicode_space(std::uint32_t cp) {
    return (cp >= 0x0009u && cp <= 0x000Du) ||
           cp == 0x0020u ||
           cp == 0x0085u ||
           cp == 0x00A0u ||
           cp == 0x1680u ||
           (cp >= 0x2000u && cp <= 0x200Au) ||
           cp == 0x2028u ||
           cp == 0x2029u ||
           cp == 0x202Fu ||
           cp == 0x205Fu ||
           cp == 0x3000u;
}

bool decode_utf8_at(std::string_view value, std::size_t pos, std::uint32_t& cp, std::size_t& width) {
    if (pos >= value.size()) return false;
    const unsigned char first = static_cast<unsigned char>(value[pos]);
    if (first < 0x80u) {
        cp = first;
        width = 1;
        return true;
    }

    std::size_t need = 0;
    std::uint32_t min_cp = 0;
    if ((first & 0xE0u) == 0xC0u) {
        need = 2; cp = first & 0x1Fu; min_cp = 0x80u;
    } else if ((first & 0xF0u) == 0xE0u) {
        need = 3; cp = first & 0x0Fu; min_cp = 0x800u;
    } else if ((first & 0xF8u) == 0xF0u) {
        need = 4; cp = first & 0x07u; min_cp = 0x10000u;
    } else {
        return false;
    }

    if (pos + need > value.size()) return false;
    for (std::size_t i = 1; i < need; ++i) {
        const unsigned char next = static_cast<unsigned char>(value[pos + i]);
        if ((next & 0xC0u) != 0x80u) return false;
        cp = (cp << 6) | (next & 0x3Fu);
    }
    if (cp < min_cp || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) return false;
    width = need;
    return true;
}

std::string normalize_unicode_whitespace(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t pos = 0; pos < value.size();) {
        std::uint32_t cp = 0;
        std::size_t width = 0;
        if (!decode_utf8_at(value, pos, cp, width)) {
            // Fail-safe preservation: malformed bytes are not silently rewritten.
            out.push_back(value[pos]);
            ++pos;
            continue;
        }
        if (unicode_space(cp)) out.push_back(' ');
        else out.append(value.substr(pos, width));
        pos += width;
    }
    return out;
}

char ascii_lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool ascii_iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    }
    return true;
}

// An embedded CUESHEET is structured metadata, not an ordinary track tag.
// Its FILE/INDEX records must only be edited by a qualified CUE-specific
// adapter. Even an explicitly targeted generic normalization rule must not
// rewrite this field as a side effect.
bool protected_cuesheet_field(std::string_view name) {
    return ascii_iequals(name, "CUESHEET") ||
           ascii_iequals(name, "CUE_SHEET") ||
           ascii_iequals(name, "__CUESHEET");
}

bool protected_lyrics_field(std::string_view name) {
    return ascii_iequals(name, "LYRICS") ||
           ascii_iequals(name, "UNSYNCEDLYRICS") ||
           ascii_iequals(name, "SYNCEDLYRICS") ||
           ascii_iequals(name, "UNSYNCED LYRICS") ||
           ascii_iequals(name, "USLT") ||
           ascii_iequals(name, "SYLT");
}

bool structured_line_breaks(std::string_view value) {
    return value.find_first_of("\r\n") != std::string_view::npos ||
           value.find("\xC2\x85") != std::string_view::npos || // U+0085 NEL
           value.find("\xE2\x80\xA8") != std::string_view::npos || // U+2028
           value.find("\xE2\x80\xA9") != std::string_view::npos;   // U+2029
}

bool generic_whitespace_transform(TransformKind kind) {
    return kind == TransformKind::NormalizeUnicodeWhitespace ||
           kind == TransformKind::TrimWhitespace ||
           kind == TransformKind::CollapseWhitespace;
}

bool field_matches(const Rule& rule, std::string_view field_name) {
    if (rule.fields.empty()) return false;
    return std::any_of(rule.fields.begin(), rule.fields.end(), [field_name](const std::string& candidate) {
        return candidate == "*" || ascii_iequals(candidate, field_name);
    });
}

bool value_matches(const Rule& rule, std::string_view value) {
    switch (rule.match) {
    case MatchKind::Always:
        return true;
    case MatchKind::Exact:
        return rule.case_sensitive ? value == rule.match_value : ascii_iequals(value, rule.match_value);
    }
    return false;
}

std::string trim_whitespace(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && ascii_space(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && ascii_space(static_cast<unsigned char>(value[end - 1]))) --end;
    return std::string(value.substr(begin, end - begin));
}

std::string collapse_whitespace(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    bool pending_space = false;
    for (unsigned char c : value) {
        if (ascii_space(c)) {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space) out.push_back(' ');
        pending_space = false;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

int safety_rank(SafetyClass value) {
    switch (value) {
    case SafetyClass::Safe: return 0;
    case SafetyClass::Confident: return 1;
    case SafetyClass::Review: return 2;
    }
    return 2;
}

std::string transform(const Rule& rule, std::string_view value) {
    switch (rule.transform) {
    case TransformKind::NormalizeUnicodeWhitespace:
        return normalize_unicode_whitespace(value);
    case TransformKind::TrimWhitespace:
        return trim_whitespace(value);
    case TransformKind::CollapseWhitespace:
        return collapse_whitespace(value);
    case TransformKind::ReplaceWith:
        return rule.replacement;
    }
    throw std::logic_error("unknown transform kind");
}

class Sha256 {
public:
    Sha256()
        : state_{
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
        } {}

    void update(std::string_view data) {
        for (unsigned char c : data) {
            buffer_[buffer_size_++] = c;
            bit_count_ += 8;
            if (buffer_size_ == 64) {
                process_block(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    std::string finish() {
        const std::uint64_t original_bits = bit_count_;
        buffer_[buffer_size_++] = 0x80u;

        if (buffer_size_ > 56) {
            while (buffer_size_ < 64) buffer_[buffer_size_++] = 0;
            process_block(buffer_.data());
            buffer_size_ = 0;
        }
        while (buffer_size_ < 56) buffer_[buffer_size_++] = 0;
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_size_++] = static_cast<unsigned char>((original_bits >> (i * 8)) & 0xffu);
        }
        process_block(buffer_.data());

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (std::uint32_t word : state_) out << std::setw(8) << word;
        return out.str();
    }

private:
    static std::uint32_t rotr(std::uint32_t value, unsigned n) {
        return (value >> n) | (value << (32 - n));
    }

    void process_block(const unsigned char* block) {
        static constexpr std::array<std::uint32_t, 64> k = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
        };

        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t j = i * 4;
            w[i] = (static_cast<std::uint32_t>(block[j]) << 24)
                 | (static_cast<std::uint32_t>(block[j + 1]) << 16)
                 | (static_cast<std::uint32_t>(block[j + 2]) << 8)
                 | static_cast<std::uint32_t>(block[j + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t bit_count_ = 0;
};

void hash_length_prefixed(Sha256& sha, std::string_view value) {
    sha.update(std::to_string(value.size()));
    sha.update(":");
    sha.update(value);
    sha.update(";");
}

} // namespace

AnalysisResult Engine::analyze(
    const MetadataDocument& input,
    const std::vector<Rule>& rules,
    std::string ruleset_revision) const {

    AnalysisResult result;
    result.input_fingerprint = fingerprint(input);
    result.ruleset_revision = std::move(ruleset_revision);
    result.canonical_preview = input;

    std::vector<const Rule*> ordered;
    ordered.reserve(rules.size());
    for (const Rule& rule : rules) {
        if (rule.enabled) ordered.push_back(&rule);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Rule* a, const Rule* b) {
        if (a->priority != b->priority) return a->priority < b->priority;
        return a->id < b->id;
    });

    for (std::size_t field_index = 0; field_index < result.canonical_preview.fields.size(); ++field_index) {
        MetadataField& field = result.canonical_preview.fields[field_index];
        if (protected_cuesheet_field(field.name))
            continue; // preserve exact embedded CUE bytes and record boundaries
        for (std::size_t value_index = 0; value_index < field.values.size(); ++value_index) {
            const std::string original = input.fields[field_index].values[value_index];
            for (const Rule* rule : ordered) {
                if (!field_matches(*rule, field.name)) continue;
                std::string& current = field.values[value_index];
                // Whitespace transformations can destroy line or stanza
                // boundaries and lyrics indentation. Never class this as
                // SAFE merely because the rule matches an "*" field.
                if (generic_whitespace_transform(rule->transform) &&
                    (protected_lyrics_field(field.name) ||
                     structured_line_breaks(current)))
                    continue;
                if (!value_matches(*rule, current)) continue;
                const std::string proposed = transform(*rule, current);
                if (proposed == current) continue;

                result.changes.push_back(Change{
                    field_index,
                    field.name,
                    value_index,
                    original,
                    current,
                    proposed,
                    rule->id,
                    rule->safety,
                    rule->rationale,
                });
                current = proposed;
            }
        }
    }

    for (std::size_t field_index = 0; field_index < result.canonical_preview.fields.size(); ++field_index) {
        const MetadataField& original_field = input.fields[field_index];
        const MetadataField& preview_field = result.canonical_preview.fields[field_index];
        for (std::size_t value_index = 0; value_index < preview_field.values.size(); ++value_index) {
            if (preview_field.values[value_index] == original_field.values[value_index]) continue;

            Proposal proposal;
            proposal.field_index = field_index;
            proposal.field = preview_field.name;
            proposal.value_index = value_index;
            proposal.original_value = original_field.values[value_index];
            proposal.proposed_value = preview_field.values[value_index];
            proposal.safety = SafetyClass::Safe;

            for (const Change& change : result.changes) {
                if (change.field_index != field_index || change.value_index != value_index) continue;
                proposal.rule_ids.push_back(change.rule_id);
                proposal.rationales.push_back(change.rationale);
                if (safety_rank(change.safety) > safety_rank(proposal.safety))
                    proposal.safety = change.safety;
            }
            result.proposals.push_back(std::move(proposal));
        }
    }
    return result;
}

std::string fingerprint(const MetadataDocument& input) {
    Sha256 sha;
    sha.update("djmeta-metadata-v1;");
    sha.update(std::to_string(input.fields.size()));
    sha.update(";");
    for (const MetadataField& field : input.fields) {
        hash_length_prefixed(sha, field.name);
        sha.update(std::to_string(field.values.size()));
        sha.update(";");
        for (const std::string& value : field.values) hash_length_prefixed(sha, value);
    }
    return sha.finish();
}

const char* to_string(SafetyClass value) {
    switch (value) {
    case SafetyClass::Safe: return "SAFE";
    case SafetyClass::Confident: return "CONFIDENT";
    case SafetyClass::Review: return "REVIEW";
    }
    return "REVIEW";
}

} // namespace djmeta
