#include "djmeta/interop.h"

#include <string>
#include <string_view>
#include <vector>

namespace djmeta {
namespace {

void append_escaped(std::string& out, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0x0F]);
                out.push_back(hex[c & 0x0F]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void append_string_array(std::string& out, const std::vector<std::string>& values) {
    out.push_back('[');
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out.push_back(',');
        append_escaped(out, values[i]);
    }
    out.push_back(']');
}

void append_metadata(std::string& out, const MetadataDocument& document) {
    out.push_back('[');
    for (std::size_t i = 0; i < document.fields.size(); ++i) {
        if (i) out.push_back(',');
        const MetadataField& field = document.fields[i];
        out += "{\"name\":";
        append_escaped(out, field.name);
        out += ",\"values\":";
        append_string_array(out, field.values);
        out.push_back('}');
    }
    out.push_back(']');
}

} // namespace

std::string serialize_analysis_json(const AnalysisResult& result) {
    std::string out;
    out.reserve(512 + result.changes.size() * 192 + result.proposals.size() * 192);
    out += "{\"schema_version\":1,\"input_fingerprint\":";
    append_escaped(out, result.input_fingerprint);
    out += ",\"ruleset_revision\":";
    append_escaped(out, result.ruleset_revision);

    out += ",\"proposals\":[";
    for (std::size_t i = 0; i < result.proposals.size(); ++i) {
        if (i) out.push_back(',');
        const Proposal& proposal = result.proposals[i];
        out += "{\"field_index\":" + std::to_string(proposal.field_index);
        out += ",\"field\":";
        append_escaped(out, proposal.field);
        out += ",\"value_index\":" + std::to_string(proposal.value_index);
        out += ",\"original\":";
        append_escaped(out, proposal.original_value);
        out += ",\"proposed\":";
        append_escaped(out, proposal.proposed_value);
        out += ",\"safety\":";
        append_escaped(out, to_string(proposal.safety));
        out += ",\"rule_ids\":";
        append_string_array(out, proposal.rule_ids);
        out += ",\"rationales\":";
        append_string_array(out, proposal.rationales);
        out.push_back('}');
    }
    out.push_back(']');

    out += ",\"trace\":[";
    for (std::size_t i = 0; i < result.changes.size(); ++i) {
        if (i) out.push_back(',');
        const Change& change = result.changes[i];
        out += "{\"field_index\":" + std::to_string(change.field_index);
        out += ",\"field\":";
        append_escaped(out, change.field);
        out += ",\"value_index\":" + std::to_string(change.value_index);
        out += ",\"original\":";
        append_escaped(out, change.original_value);
        out += ",\"before\":";
        append_escaped(out, change.before_value);
        out += ",\"proposed\":";
        append_escaped(out, change.proposed_value);
        out += ",\"rule_id\":";
        append_escaped(out, change.rule_id);
        out += ",\"safety\":";
        append_escaped(out, to_string(change.safety));
        out += ",\"rationale\":";
        append_escaped(out, change.rationale);
        out.push_back('}');
    }
    out.push_back(']');

    out += ",\"canonical_preview\":";
    append_metadata(out, result.canonical_preview);
    out.push_back('}');
    return out;
}

} // namespace djmeta
