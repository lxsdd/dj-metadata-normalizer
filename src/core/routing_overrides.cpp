#include "djmeta/routing_overrides.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace djmeta {
namespace {
bool invalid_text(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return c == 0 || c == '\r' || c == '\n' || c < 0x20u || c == 0x7fu;
    });
}
void invalidate_target(FilePlanItem& item) {
    item.target_path.clear();
    item.target_key.clear();
    item.target_presence = TargetPresence::Unchecked;
    item.target_guard.clear();
    item.cue_links = CueLinkState::Unchecked;
    item.cue_postimage_fingerprint.clear();
    item.cue_references_will_change = false;
}
} // namespace

RoutingOverrideResult apply_routing_override(
    const std::vector<FilePlanItem>& original,
    const RoutingOverride& change) {

    RoutingOverrideResult result;
    const bool needs_naming = change.action != FileAction::None;
    if ((needs_naming &&
         (change.routing_profile.empty() || change.naming_expression.empty())) ||
        invalid_text(change.routing_profile) ||
        invalid_text(change.naming_expression)) {
        result.diagnostics.emplace_back("INVALID_ROUTE_OR_NAMING_EXPRESSION");
        return result;
    }

    std::set<std::string> requested_ids;
    if (change.scope == RoutingScope::SelectedAudio) {
        if (change.selected_physical_ids.empty()) {
            result.diagnostics.emplace_back("NO_SELECTED_PHYSICAL_AUDIO");
            return result;
        }
        for (const auto& id : change.selected_physical_ids) {
            if (id.empty() || !requested_ids.insert(id).second) {
                result.diagnostics.emplace_back("INVALID_OR_DUPLICATE_SELECTED_AUDIO_ID");
                return result;
            }
        }
    } else if (!change.selected_physical_ids.empty()) {
        result.diagnostics.emplace_back("ALL_AUDIO_SCOPE_HAS_SELECTED_IDS");
        return result;
    }

    std::set<std::string> available_audio_ids;
    for (const auto& item : original) {
        if (item.role != FileRole::Audio) continue;
        if (item.physical_id.empty() || !available_audio_ids.insert(item.physical_id).second) {
            result.diagnostics.emplace_back("AMBIGUOUS_PHYSICAL_AUDIO_IDENTITY");
            return result;
        }
    }
    if (available_audio_ids.empty()) {
        result.diagnostics.emplace_back("BATCH_CONTAINS_NO_AUDIO");
        return result;
    }
    if (change.scope == RoutingScope::SelectedAudio) {
        for (const auto& id : requested_ids) {
            if (available_audio_ids.count(id) == 0) {
                result.diagnostics.emplace_back("SELECTED_AUDIO_NOT_IN_BATCH");
                return result;
            }
        }
    } else {
        requested_ids = available_audio_ids;
    }

    result.items = original; // never mutate original snapshot
    for (auto& item : result.items) {
        if (item.role != FileRole::Audio ||
            requested_ids.count(item.physical_id) == 0)
            continue;

        // An explicit override marks user intent, even if its value happens
        // to match a previous default; this changes the approval fingerprint.
        item.routing_profile = change.routing_profile;
        item.naming_expression = change.naming_expression;
        item.action = change.action;
        item.manual_override = true;
        invalidate_target(item);
        ++result.changed_audio_count;
    }

    for (auto& item : result.items) {
        if (item.role == FileRole::Audio) continue;

        // The current plan item records one primary associated_audio_id,
        // but an external multi-FILE cue can reference MANY audio sources.
        // To avoid retaining a stale shared CUE plan when any one source
        // changes route, invalidate EVERY external CUE in this batch.
        // Once the host supplies the full reference-to-audio set, this can
        // be narrowed without weakening the safety contract.
        if (item.role == FileRole::ExternalCue ||
            requested_ids.count(item.associated_audio_id) != 0) {
            invalidate_target(item);
        }
    }

    result.accepted = true;
    return result;
}

} // namespace djmeta
