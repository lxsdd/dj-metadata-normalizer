#include "djmeta/batch_plan.h"
#include "djmeta/normalizer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace djmeta {
namespace {

std::string decimal(int value) { return std::to_string(value); }
std::string yesno(bool value) { return value ? "1" : "0"; }

std::string plan_fingerprint(const std::vector<FilePlanItem>& items) {
    MetadataDocument document;
    document.fields.push_back({"PLAN_CONTRACT", {"djmeta-batch-v1"}});
    for (const FilePlanItem& item : items) {
        // The existing SHA-256 metadata fingerprint uses explicit lengths for
        // every field/value and includes field ordering. Never concatenate
        // unescaped strings to construct approval identity.
        document.fields.push_back({"ITEM", {
            item.physical_id,
            item.source_path, item.source_key,
            item.target_path, item.target_key,
            decimal(static_cast<int>(item.action)),
            decimal(static_cast<int>(item.role)),
            item.associated_audio_id,
            item.metadata_fingerprint, item.planned_metadata_fingerprint,
            item.ruleset_revision,
            item.routing_profile, item.naming_expression,
            yesno(item.manual_override),
            yesno(item.companion_policy_qualified),
            decimal(static_cast<int>(item.target_presence)),
            item.target_guard,
            decimal(static_cast<int>(item.cue_links)),
            item.cue_source_fingerprint, item.cue_postimage_fingerprint,
            yesno(item.cue_references_will_change)
        }});
    }
    return fingerprint(document);
}

void block(ItemDecision& decision, const char* reason) {
    decision.reasons.emplace_back(reason);
    decision.status = PlanStatus::Blocked;
}

// Unrecognized serialized/host-provided enum values must never inherit
// execution semantics from a default branch. The planner fails closed for
// every selected row, including a tag-only/no-file-action item.
bool valid_action(FileAction value) {
    switch (value) {
    case FileAction::None:
    case FileAction::Rename:
    case FileAction::Move:
    case FileAction::Copy: return true;
    default: return false;
    }
}
bool valid_role(FileRole value) {
    switch (value) {
    case FileRole::Audio:
    case FileRole::ExternalCue:
    case FileRole::Companion: return true;
    default: return false;
    }
}
bool valid_presence(TargetPresence value) {
    switch (value) {
    case TargetPresence::Unchecked:
    case TargetPresence::Missing:
    case TargetPresence::Existing: return true;
    default: return false;
    }
}
bool valid_cue_state(CueLinkState value) {
    switch (value) {
    case CueLinkState::Unchecked:
    case CueLinkState::NoExternalCue:
    case CueLinkState::Verified:
    case CueLinkState::Unresolved: return true;
    default: return false;
    }
}

bool active_file_action(const FilePlanItem& item) {
    return item.action != FileAction::None;
}

bool active_item(const FilePlanItem& item) {
    return active_file_action(item) || item.cue_references_will_change;
}

} // namespace

BatchPlanReview review_batch_plan(
    const std::vector<FilePlanItem>& items,
    const BatchApproval* approval) {

    BatchPlanReview review;
    review.plan_fingerprint = plan_fingerprint(items);
    review.decisions.resize(items.size());
    const bool stale_approval =
        approval != nullptr &&
        approval->reviewed_plan_fingerprint != review.plan_fingerprint;
    const bool blanket_overwrite =
        approval != nullptr && !stale_approval &&
        approval->approve_all_observed_overwrites;

    std::map<std::string, std::size_t> source_key_counts;
    std::map<std::string, std::size_t> physical_id_counts;
    std::map<std::string, std::size_t> target_key_counts;
    std::set<std::string> audio_ids;
    std::map<std::string, CueLinkState> audio_cue_states;
    std::set<std::string> all_source_keys;
    std::map<std::string, std::size_t> cue_counts;

    for (const FilePlanItem& item : items) {
        if (!item.source_key.empty()) all_source_keys.insert(item.source_key);
        if (item.role == FileRole::Audio && !item.physical_id.empty()) {
            audio_ids.insert(item.physical_id);
            // Preserve the host-qualified relationship state of every audio
            // item, even when selected only for metadata updates.
            audio_cue_states[item.physical_id] = item.cue_links;
        }
        if (!active_item(item)) continue;
        if (!item.source_key.empty()) ++source_key_counts[item.source_key];
        if (!item.physical_id.empty()) ++physical_id_counts[item.physical_id];
        if (active_file_action(item) && !item.target_key.empty())
            ++target_key_counts[item.target_key];
        if (item.role == FileRole::ExternalCue && !item.associated_audio_id.empty())
            ++cue_counts[item.associated_audio_id];
    }

    for (std::size_t index = 0; index < items.size(); ++index) {
        const FilePlanItem& item = items[index];
        ItemDecision& decision = review.decisions[index];
        decision.status = PlanStatus::Ready;

        if (!valid_action(item.action) || !valid_role(item.role) ||
            !valid_presence(item.target_presence) || !valid_cue_state(item.cue_links)) {
            block(decision, "UNKNOWN_PLAN_ENUM_VALUE");
            if (stale_approval) block(decision, "STALE_BATCH_APPROVAL");
            ++review.blocked;
            continue;
        }

        if (item.cue_references_will_change && item.role != FileRole::ExternalCue)
            block(decision, "CUE_REWRITE_ON_NON_CUE_ITEM");

        if (!active_item(item)) {
            // A stale plan approval must invalidate the *entire* batch,
            // including tag-only and otherwise unchanged file rows. Do not
            // silently skip approval identity checking via this early exit.
            if (stale_approval) {
                block(decision, "STALE_BATCH_APPROVAL");
                ++review.blocked;
            } else {
                decision.status = PlanStatus::Unchanged;
            }
            continue;
        }

        if (item.physical_id.empty() || item.source_path.empty() ||
            item.source_key.empty() || item.ruleset_revision.empty()) {
            block(decision, "MISSING_SOURCE_OR_RULESET_IDENTITY");
        }
        if (item.role == FileRole::Audio &&
            (item.metadata_fingerprint.empty() || item.planned_metadata_fingerprint.empty()))
            block(decision, "MISSING_METADATA_FINGERPRINT");

        if (!item.source_key.empty() && source_key_counts[item.source_key] > 1)
            block(decision, "DUPLICATE_PHYSICAL_SOURCE");
        if (!item.physical_id.empty() && physical_id_counts[item.physical_id] > 1)
            block(decision, "DUPLICATE_PHYSICAL_ID");

        if (item.role == FileRole::Audio) {
            if (item.cue_links == CueLinkState::NoExternalCue &&
                cue_counts[item.physical_id] != 0)
                block(decision, "CUE_ASSOCIATION_CONTRADICTS_NO_EXTERNAL_CUE");
            if (item.cue_links == CueLinkState::Unchecked ||
                item.cue_links == CueLinkState::Unresolved) {
                block(decision, "CUE_DEPENDENCY_UNQUALIFIED");
            } else if (item.cue_links == CueLinkState::Verified &&
                       cue_counts[item.physical_id] == 0) {
                block(decision, "VERIFIED_CUE_NOT_IN_PLAN");
            }
        } else if (item.role == FileRole::Companion) {
            if (item.associated_audio_id.empty() ||
                audio_ids.count(item.associated_audio_id) == 0 ||
                (!item.companion_policy_qualified && !item.manual_override))
                block(decision, "COMPANION_NOT_AUTHORIZED");
        } else if (item.role == FileRole::ExternalCue) {
            if (item.associated_audio_id.empty() ||
                audio_ids.count(item.associated_audio_id) == 0 ||
                audio_cue_states[item.associated_audio_id] != CueLinkState::Verified ||
                item.cue_links != CueLinkState::Verified ||
                item.cue_source_fingerprint.empty() ||
                item.cue_postimage_fingerprint.empty()) {
                block(decision, "CUE_REFERENCE_PLAN_UNQUALIFIED");
            }
        }

        if (active_file_action(item)) {
            if (item.target_path.empty() || item.target_key.empty())
                block(decision, "MISSING_TARGET_PATH_OR_IDENTITY");
            if (!item.target_key.empty() && target_key_counts[item.target_key] > 1)
                block(decision, "MULTIPLE_SOURCES_SAME_TARGET");
            if (!item.target_key.empty() && item.target_key != item.source_key &&
                all_source_keys.count(item.target_key) != 0) {
                block(decision, "TARGET_IS_BATCH_SOURCE");
            }
            if (!item.target_key.empty() && item.target_key == item.source_key) {
                if (item.action == FileAction::Copy)
                    block(decision, "COPY_SOURCE_EQUALS_TARGET");
                else if (!item.cue_references_will_change) {
                    // No physical rename/move is needed.
                    decision.status = decision.reasons.empty()
                        ? PlanStatus::Unchanged : PlanStatus::Blocked;
                    if (stale_approval) block(decision, "STALE_BATCH_APPROVAL");
                    if (decision.status == PlanStatus::Blocked) ++review.blocked;
                    continue;
                }
            }

            if (item.target_presence == TargetPresence::Unchecked)
                block(decision, "TARGET_NOT_INSPECTED");
            else if (item.target_presence == TargetPresence::Existing) {
                if (item.target_guard.empty()) {
                    block(decision, "EXISTING_TARGET_NOT_GUARDED");
                } else {
                    ++review.requires_overwrite;
                    const bool individually_approved = approval != nullptr &&
                        !stale_approval &&
                        std::find(approval->individually_approved_physical_ids.begin(),
                                  approval->individually_approved_physical_ids.end(),
                                  item.physical_id) !=
                            approval->individually_approved_physical_ids.end();
                    const bool overwrite_allowed =
                        individually_approved || blanket_overwrite;
                    if (overwrite_allowed) {
                        decision.will_replace_existing_target = true;
                    } else if (decision.status != PlanStatus::Blocked) {
                        decision.status = PlanStatus::NeedsOverwriteApproval;
                        decision.reasons.push_back("EXISTING_TARGET_REQUIRES_APPROVAL");
                    }
                }
            }
        }

        if (stale_approval)
            block(decision, "STALE_BATCH_APPROVAL");
        if (!decision.reasons.empty() &&
            decision.status != PlanStatus::NeedsOverwriteApproval)
            decision.status = PlanStatus::Blocked;
        if (decision.status == PlanStatus::Blocked) {
            decision.will_replace_existing_target = false;
            ++review.blocked;
        }
    }

    review.ready_to_apply = !items.empty() && review.blocked == 0 &&
        std::all_of(review.decisions.begin(), review.decisions.end(),
            [](const ItemDecision& item) {
                return item.status == PlanStatus::Ready ||
                       item.status == PlanStatus::Unchanged;
            });
    return review;
}

} // namespace djmeta
