#include "stdafx.h"

#include "metadata_adapter.h"
#include "preview.h"
#include "rules_runtime.h"

#include <algorithm>

namespace djmeta_foobar {
namespace {

std::string display_value(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (c == '\r' || c == '\n' || c == '\t' || c == 0) out.push_back(' ');
        else out.push_back(static_cast<char>(c));
    }
    return out;
}

void append_rules(std::string& out, const std::vector<std::string>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i) out += ", ";
        out += ids[i];
    }
}

} // namespace

void show_normalization_preview(const metadb_handle_list& handles) {
    try {
        const loaded_rules_text loaded = load_rules_text();
        const djmeta::Ruleset ruleset = djmeta::parse_ruleset_json(loaded.json);

        std::string detail;
        std::size_t proposal_count = 0;
        std::size_t safe_count = 0;
        std::size_t confident_count = 0;
        std::size_t review_count = 0;
        constexpr std::size_t kDetailLimit = 80;

        for (t_size item_index = 0; item_index < handles.get_count(); ++item_index) {
            const metadb_handle_ptr& handle = handles[item_index];
            const auto container = handle->get_info_ref();
            const file_info& info = container->info();
            const djmeta::MetadataDocument input = metadata_from_file_info(info);
            const djmeta::AnalysisResult result =
                djmeta::Engine{}.analyze(input, ruleset.rules, ruleset.revision);

            for (const djmeta::Proposal& proposal : result.proposals) {
                ++proposal_count;
                switch (proposal.safety) {
                case djmeta::SafetyClass::Safe: ++safe_count; break;
                case djmeta::SafetyClass::Confident: ++confident_count; break;
                case djmeta::SafetyClass::Review: ++review_count; break;
                }

                if (proposal_count > kDetailLimit) continue;

                detail += "\n";
                detail += handle->get_path();
                detail += " [";
                detail += std::to_string(handle->get_subsong_index());
                detail += "]\n  [";
                detail += djmeta::to_string(proposal.safety);
                detail += "] ";
                detail += proposal.field;
                detail += ": \"";
                detail += display_value(proposal.original_value);
                detail += "\" -> \"";
                detail += display_value(proposal.proposed_value);
                detail += "\"\n  Regeln: ";
                append_rules(detail, proposal.rule_ids);
                detail += "\n";
            }
        }

        std::string message;
        message += "Regelbestand: ";
        message += loaded.source_label;
        message += "\nRevision: ";
        message += ruleset.revision;
        message += "\nAusgewählte Tracks: ";
        message += std::to_string(handles.get_count());
        message += "\nVorschläge: ";
        message += std::to_string(proposal_count);
        message += " (SAFE ";
        message += std::to_string(safe_count);
        message += ", CONFIDENT ";
        message += std::to_string(confident_count);
        message += ", REVIEW ";
        message += std::to_string(review_count);
        message += ")\n";

        if (proposal_count == 0) {
            message += "\nKeine Normalisierungsvorschläge. Es wurde nichts verändert.";
        } else {
            message += detail;
            if (proposal_count > kDetailLimit) {
                message += "\n... ";
                message += std::to_string(proposal_count - kDetailLimit);
                message += " weitere Vorschläge werden in dieser frühen Vorschau nicht angezeigt.";
            }
            message += "\n\nNur Vorschau: Es wurden keine Tags geschrieben.";
        }

        popup_message::g_show(message.c_str(), "DJ Metadata Normalizer");
    } catch (const std::exception& error) {
        std::string message =
            "Die Normalisierungsvorschau konnte nicht erstellt werden. Es wurde nichts verändert.\n\n";
        message += error.what();
        popup_message::g_show(message.c_str(), "DJ Metadata Normalizer");
    }
}

} // namespace djmeta_foobar
