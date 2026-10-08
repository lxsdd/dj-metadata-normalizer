#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace djmeta {

// A pure, read-only preflight model. The foobar adapter supplies canonical
// identity keys and observed destination identities through host filesystem
// services; this core NEVER probes the filesystem or executes operations.
enum class FileAction { None, Rename, Move, Copy };
enum class FileRole { Audio, ExternalCue, Companion };
enum class TargetPresence { Unchecked, Missing, Existing };
enum class CueLinkState { Unchecked, NoExternalCue, Verified, Unresolved };
enum class PlanStatus { Ready, Unchanged, NeedsOverwriteApproval, Blocked };

struct FilePlanItem {
    // Stable canonical physical file identity. Never use a subsong ID here.
    std::string physical_id;
    std::string source_path;
    std::string source_key;
    std::string target_path;
    std::string target_key;
    FileAction action = FileAction::None;
    FileRole role = FileRole::Audio;
    // For a CUE entry: physical_id of its parent/related audio item.
    std::string associated_audio_id;

    std::string metadata_fingerprint;
    // Approved/selected proposal postimage, not merely raw input fingerprint.
    std::string planned_metadata_fingerprint;
    std::string ruleset_revision;
    std::string routing_profile;
    std::string naming_expression;
    bool manual_override = false;

    // Result of host filesystem preflight, not inferred by this engine.
    TargetPresence target_presence = TargetPresence::Unchecked;
    // Host-observed destination identity/stat guard, required if Existing.
    std::string target_guard;
    // A per-item explicit overwrite selection; independent of batch approval.
    bool individual_overwrite_approved = false;

    // An audio file must explicitly report no external cue, or verified cue
    // handling. Unchecked and unresolved dependencies are always blocked.
    CueLinkState cue_links = CueLinkState::Unchecked;
    // For ExternalCue: the exact source bytes and projected FILE-reference
    // postimage must be qualified by a separate read-only cue parser.
    std::string cue_source_fingerprint;
    std::string cue_postimage_fingerprint;
    bool cue_references_will_change = false;
};

struct BatchApproval {
    // Bound to the exact item and filesystem-observation snapshot.
    std::string reviewed_plan_fingerprint;
    bool approve_all_observed_overwrites = false;
};

struct ItemDecision {
    PlanStatus status = PlanStatus::Blocked;
    // Stable diagnostic codes for the future foobar/DJ Library UI.
    std::vector<std::string> reasons;
    bool will_replace_existing_target = false;
};

struct BatchPlanReview {
    std::string plan_fingerprint;
    std::vector<ItemDecision> decisions;
    std::size_t requires_overwrite = 0;
    std::size_t blocked = 0;
    bool ready_to_apply = false; // advisory only; executor MUST re-probe
};

// Deterministic, no-IO review. Approval with a nonmatching fingerprint fails
// closed even if only one route, CUE reference, or observed target changed.
BatchPlanReview review_batch_plan(
    const std::vector<FilePlanItem>& items,
    const BatchApproval* approval = nullptr);

} // namespace djmeta
