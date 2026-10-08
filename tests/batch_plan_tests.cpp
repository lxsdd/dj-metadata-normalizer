#include "djmeta/batch_plan.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool ok, const char* reason) {
    if (!ok) {
        std::cerr << "FAIL: " << reason << "\n";
        std::exit(1);
    }
}

bool has_reason(const djmeta::ItemDecision& decision, const std::string& code) {
    for (const auto& reason : decision.reasons) {
        if (reason == code) return true;
    }
    return false;
}

djmeta::FilePlanItem audio(const std::string& name) {
    djmeta::FilePlanItem item;
    item.physical_id = name;
    item.source_path = "Z:/Music/Downloads/" + name + ".mp3";
    item.source_key = "z:/music/downloads/" + name + ".mp3";
    item.target_path = "Z:/Music/Singles/" + name + ".mp3";
    item.target_key = "z:/music/singles/" + name + ".mp3";
    item.action = djmeta::FileAction::Move;
    item.role = djmeta::FileRole::Audio;
    item.metadata_fingerprint = "raw-" + name;
    item.planned_metadata_fingerprint = "canonical-" + name;
    item.ruleset_revision = "2026-10-07.2";
    item.routing_profile = "Singles";
    item.naming_expression = "%artist% - %title%";
    item.target_presence = djmeta::TargetPresence::Missing;
    item.cue_links = djmeta::CueLinkState::NoExternalCue;
    return item;
}

void test_ready_and_immutable() {
    const std::vector<djmeta::FilePlanItem> batch = {audio("A"), audio("B")};
    const auto copy = batch;
    const auto result = djmeta::review_batch_plan(batch);
    require(batch[0].source_path == copy[0].source_path, "planner mutated input");
    require(result.plan_fingerprint.size() == 64, "plan fingerprint must be SHA-256");
    require(result.ready_to_apply && result.blocked == 0 &&
            result.requires_overwrite == 0, "clean batch must be ready");
    require(result.decisions[0].status == djmeta::PlanStatus::Ready,
            "clean item should be ready");
    require(djmeta::review_batch_plan(batch).plan_fingerprint == result.plan_fingerprint,
            "plan fingerprint must be deterministic");

    auto changed = batch;
    changed[0].manual_override = true;
    require(djmeta::review_batch_plan(changed).plan_fingerprint != result.plan_fingerprint,
            "manual routing override must change plan fingerprint");
    changed = batch;
    changed[0].planned_metadata_fingerprint = "different selected postimage";
    require(djmeta::review_batch_plan(changed).plan_fingerprint != result.plan_fingerprint,
            "selected metadata proposals must bind plan");
    changed = batch;
    changed[0].target_path += ".new";
    require(djmeta::review_batch_plan(changed).plan_fingerprint != result.plan_fingerprint,
            "target path edit must change plan");
    changed = batch;
    changed[0].routing_profile = "Alben";
    require(djmeta::review_batch_plan(changed).plan_fingerprint != result.plan_fingerprint,
            "routing profile edit must change plan");
}

void test_large_batch_one_confirmation() {
    std::vector<djmeta::FilePlanItem> batch;
    for (int i = 0; i < 100; ++i) {
        auto item = audio("Track" + std::to_string(i));
        item.target_presence = djmeta::TargetPresence::Existing;
        item.target_guard = "guard-" + std::to_string(i);
        batch.push_back(std::move(item));
    }
    const auto preview = djmeta::review_batch_plan(batch);
    require(preview.requires_overwrite == 100 &&
            !preview.ready_to_apply && preview.blocked == 0,
            "100 existing targets should yield 100 visible warnings, not blocks");

    const djmeta::BatchApproval approval{preview.plan_fingerprint, true};
    const auto approved = djmeta::review_batch_plan(batch, &approval);
    require(approved.ready_to_apply && approved.requires_overwrite == 100,
            "one batch-wide approval should approve all 100 reviewed replacements");
    for (const auto& item : approved.decisions)
        require(item.will_replace_existing_target,
                "all 100 reviewed replacements should be approved");
}

void test_batch_overwrite_once_and_target_guard() {
    auto a = audio("A");
    auto b = audio("B");
    a.target_presence = djmeta::TargetPresence::Existing;
    a.target_guard = "target-A-content-v1";
    b.target_presence = djmeta::TargetPresence::Existing;
    b.target_guard = "target-B-content-v1";
    std::vector<djmeta::FilePlanItem> batch = {a, b};

    const auto preview = djmeta::review_batch_plan(batch);
    require(!preview.ready_to_apply && preview.blocked == 0 &&
            preview.requires_overwrite == 2, "two existing targets require one batch consent");
    require(preview.decisions[0].status == djmeta::PlanStatus::NeedsOverwriteApproval,
            "existing target must raise a warning");

    const djmeta::BatchApproval approval{preview.plan_fingerprint, true};
    const auto approved = djmeta::review_batch_plan(batch, &approval);
    require(approved.ready_to_apply && approved.requires_overwrite == 2 &&
            approved.decisions[0].will_replace_existing_target &&
            approved.decisions[1].will_replace_existing_target,
            "one explicit batch approval must authorize both existing targets");

    batch[1].target_guard = "target-B-content-v2";
    const auto stale = djmeta::review_batch_plan(batch, &approval);
    require(!stale.ready_to_apply && stale.blocked == 2 &&
            has_reason(stale.decisions[0], "STALE_BATCH_APPROVAL") &&
            has_reason(stale.decisions[1], "STALE_BATCH_APPROVAL"),
            "changed destination guard must invalidate entire approval");

    batch = {a, b};
    djmeta::BatchApproval individual{
        djmeta::review_batch_plan(batch).plan_fingerprint, false, {"A"}};
    const auto mixed = djmeta::review_batch_plan(batch, &individual);
    require(mixed.decisions[0].status == djmeta::PlanStatus::Ready &&
            mixed.decisions[1].status == djmeta::PlanStatus::NeedsOverwriteApproval,
            "per-item consent must remain available");

    batch[0].target_guard += "-updated";
    const auto stale_individual = djmeta::review_batch_plan(batch, &individual);
    require(stale_individual.blocked == 2 &&
            !stale_individual.decisions[0].will_replace_existing_target,
            "per-item overwrite approval must also be guarded by plan fingerprint");
}

void test_missing_target_guard_and_no_inspection() {
    auto a = audio("A");
    a.target_presence = djmeta::TargetPresence::Unchecked;
    auto result = djmeta::review_batch_plan({a});
    require(result.blocked == 1 &&
            has_reason(result.decisions[0], "TARGET_NOT_INSPECTED"),
            "uninspected destination must fail closed");

    a.target_presence = djmeta::TargetPresence::Existing;
    a.target_guard.clear();
    result = djmeta::review_batch_plan({a});
    require(result.blocked == 1 &&
            has_reason(result.decisions[0], "EXISTING_TARGET_NOT_GUARDED"),
            "existing target without identity guard must fail closed");
}

void test_new_target_after_clean_review_requires_new_consent() {
    auto item = audio("A"); // target absent at preview time
    const auto preview = djmeta::review_batch_plan({item});
    require(preview.ready_to_apply, "absent target should initially be plannable");
    const djmeta::BatchApproval prior{preview.plan_fingerprint, true};

    // A third party creates the file between the preview and Apply.
    item.target_presence = djmeta::TargetPresence::Existing;
    item.target_guard = "newly-created-target";
    const auto changed = djmeta::review_batch_plan({item}, &prior);
    require(changed.blocked == 1 && !changed.ready_to_apply &&
            !changed.decisions[0].will_replace_existing_target &&
            has_reason(changed.decisions[0], "STALE_BATCH_APPROVAL"),
            "new destination after approval must not be silently overwritten");
}

void test_stale_approval_rejects_tag_only_and_noop_rows() {
    auto a = audio("TagOnly-A");
    a.action = djmeta::FileAction::None;
    a.target_key.clear();
    a.target_path.clear();
    a.target_presence = djmeta::TargetPresence::Unchecked;

    auto b = audio("TagOnly-B");
    b.action = djmeta::FileAction::None;
    b.target_key.clear();
    b.target_path.clear();

    const std::vector<djmeta::FilePlanItem> batch{a, b};
    const auto initial = djmeta::review_batch_plan(batch);
    require(initial.ready_to_apply && initial.blocked == 0 &&
            initial.decisions[0].status == djmeta::PlanStatus::Unchanged &&
            initial.decisions[1].status == djmeta::PlanStatus::Unchanged,
            "tag-only file plan must stay advisory and unchanged without a stale approval");

    const djmeta::BatchApproval valid{initial.plan_fingerprint, false};
    const auto same = djmeta::review_batch_plan(batch, &valid);
    require(same.ready_to_apply && same.blocked == 0,
            "matching approval must not block a legitimate unchanged file plan");

    auto mutated = batch;
    mutated[0].planned_metadata_fingerprint = "new-in-memory-tag-review";
    const auto stale = djmeta::review_batch_plan(mutated, &valid);
    require(!stale.ready_to_apply && stale.blocked == 2 &&
            has_reason(stale.decisions[0], "STALE_BATCH_APPROVAL") &&
            has_reason(stale.decisions[1], "STALE_BATCH_APPROVAL"),
            "changed tag postimage invalidates all batch approvals, even tag-only rows");

    mutated = batch;
    mutated[1].ruleset_revision = "new-rules-version";
    const auto stale_rules = djmeta::review_batch_plan(mutated, &valid);
    require(!stale_rules.ready_to_apply && stale_rules.blocked == 2 &&
            has_reason(stale_rules.decisions[0], "STALE_BATCH_APPROVAL") &&
            has_reason(stale_rules.decisions[1], "STALE_BATCH_APPROVAL"),
            "changed ruleset revision invalidates unchanged file rows too");

    // One active move plus a tag-only row: no silently excluded item
    // may escape the shared plan identity guard.
    mutated = batch;
    mutated[1].action = djmeta::FileAction::Move;
    mutated[1].target_path = "Z:/Music/Singles/TagOnly-B.mp3";
    mutated[1].target_key = "z:/music/singles/tagonly-b.mp3";
    mutated[1].target_presence = djmeta::TargetPresence::Missing;
    const auto mixed = djmeta::review_batch_plan(mutated, &valid);
    require(mixed.blocked == 2 && !mixed.ready_to_apply &&
            has_reason(mixed.decisions[0], "STALE_BATCH_APPROVAL") &&
            has_reason(mixed.decisions[1], "STALE_BATCH_APPROVAL"),
            "mixed move/tag-only stale batch cannot partially escape approval guard");
}

void test_intra_batch_conflicts_cannot_be_overridden() {
    auto a = audio("A");
    auto b = audio("B");
    b.target_key = a.target_key;
    b.target_path = a.target_path;
    auto batch = std::vector<djmeta::FilePlanItem>{a, b};
    auto preview = djmeta::review_batch_plan(batch);
    require(preview.blocked == 2 &&
            has_reason(preview.decisions[0], "MULTIPLE_SOURCES_SAME_TARGET"),
            "different sources with same destination must be blocked");

    const djmeta::BatchApproval allow{preview.plan_fingerprint, true};
    const auto still_blocked = djmeta::review_batch_plan(batch, &allow);
    require(still_blocked.blocked == 2 && !still_blocked.ready_to_apply,
            "batch overwrite must not pick a winning source");

    b = audio("B");
    b.action = djmeta::FileAction::None; // selected for tag-only updates
    a.target_key = b.source_key;
    a.target_path = b.source_path;
    preview = djmeta::review_batch_plan({a, b});
    require(has_reason(preview.decisions[0], "TARGET_IS_BATCH_SOURCE"),
            "cannot replace a selected tag-only file by a batch move");
}

void test_duplicate_subsong_source_and_self_copy() {
    auto a = audio("A");
    auto b = audio("B");
    b.source_key = a.source_key; // two virtual tracks, same physical file
    auto result = djmeta::review_batch_plan({a, b});
    require(result.blocked == 2 &&
            has_reason(result.decisions[0], "DUPLICATE_PHYSICAL_SOURCE"),
            "two virtual subsongs must not produce two file operations");

    a = audio("A");
    a.action = djmeta::FileAction::Copy;
    a.target_path = a.source_path;
    a.target_key = a.source_key;
    result = djmeta::review_batch_plan({a});
    require(result.blocked == 1 &&
            has_reason(result.decisions[0], "COPY_SOURCE_EQUALS_TARGET"),
            "copy-to-self must not be allowed");
}

void test_external_cue_dependencies() {
    auto a = audio("A");
    a.cue_links = djmeta::CueLinkState::Unchecked;
    auto result = djmeta::review_batch_plan({a});
    require(result.blocked == 1 &&
            has_reason(result.decisions[0], "CUE_DEPENDENCY_UNQUALIFIED"),
            "audio rename without cue inspection must be blocked");

    a.cue_links = djmeta::CueLinkState::Verified;
    result = djmeta::review_batch_plan({a});
    require(has_reason(result.decisions[0], "VERIFIED_CUE_NOT_IN_PLAN"),
            "verified cue must be represented by a separate planned item");

    djmeta::FilePlanItem cue;
    cue.physical_id = "cue-A";
    cue.source_path = "Z:/Music/Downloads/A.cue";
    cue.source_key = "z:/music/downloads/a.cue";
    cue.target_path = "Z:/Music/Singles/A.cue";
    cue.target_key = "z:/music/singles/a.cue";
    cue.action = djmeta::FileAction::Move;
    cue.role = djmeta::FileRole::ExternalCue;
    cue.associated_audio_id = a.physical_id;
    cue.ruleset_revision = a.ruleset_revision;
    cue.cue_links = djmeta::CueLinkState::Verified;
    cue.cue_source_fingerprint = "cue-exact-source-bytes";
    cue.cue_postimage_fingerprint = "cue-renamed-FILE-reference-bytes";
    cue.cue_references_will_change = true;
    cue.target_presence = djmeta::TargetPresence::Missing;

    std::vector<djmeta::FilePlanItem> batch = {a, cue};
    result = djmeta::review_batch_plan(batch);
    require(result.ready_to_apply, "verified CUE + audio move should be plannable");
    const djmeta::BatchApproval approval{result.plan_fingerprint, true};

    batch[1].cue_postimage_fingerprint += "changed";
    result = djmeta::review_batch_plan(batch, &approval);
    require(result.blocked == 2 &&
            has_reason(result.decisions[1], "STALE_BATCH_APPROVAL"),
            "modified CUE FILE reference plan must invalidate approval");

    batch[1].cue_source_fingerprint.clear();
    result = djmeta::review_batch_plan(batch);
    require(has_reason(result.decisions[1], "CUE_REFERENCE_PLAN_UNQUALIFIED"),
            "unverified CUE source bytes must block operation");
}

void test_contradictory_audio_cue_evidence_rejected() {
    auto source = audio("contradictory");
    djmeta::FilePlanItem cue;
    cue.physical_id = "contradictory-cue";
    cue.source_path = "Z:/Music/Downloads/contradictory.cue";
    cue.source_key = "z:/music/downloads/contradictory.cue";
    cue.target_path = "Z:/Music/Singles/contradictory.cue";
    cue.target_key = "z:/music/singles/contradictory.cue";
    cue.action = djmeta::FileAction::Move;
    cue.role = djmeta::FileRole::ExternalCue;
    cue.associated_audio_id = source.physical_id;
    cue.ruleset_revision = source.ruleset_revision;
    cue.cue_links = djmeta::CueLinkState::Verified;
    cue.cue_source_fingerprint = "verified-source-cue-bytes";
    cue.cue_postimage_fingerprint = "verified-proposed-cue-bytes";
    cue.cue_references_will_change = true;
    cue.target_presence = djmeta::TargetPresence::Missing;

    // The host cannot simultaneously assert "no external CUE" and supply
    // an explicitly associated, active CUE dependency in the same plan.
    const auto contradict = djmeta::review_batch_plan({source, cue});
    require(contradict.blocked == 2 && !contradict.ready_to_apply &&
            has_reason(contradict.decisions[0],
                "CUE_ASSOCIATION_CONTRADICTS_NO_EXTERNAL_CUE") &&
            has_reason(contradict.decisions[1],
                "CUE_REFERENCE_PLAN_UNQUALIFIED"),
            "contradictory audio/CUE association must block both sides");

    const djmeta::BatchApproval approval{contradict.plan_fingerprint, true};
    const auto still_blocked = djmeta::review_batch_plan({source, cue}, &approval);
    require(still_blocked.blocked == 2 && !still_blocked.ready_to_apply,
            "blanket overwrite approval cannot repair CUE identity contradiction");

    source.cue_links = djmeta::CueLinkState::Verified;
    const auto qualified = djmeta::review_batch_plan({source, cue});
    require(qualified.ready_to_apply && qualified.blocked == 0,
            "mutually qualified CUE and audio evidence must retain a clean preview");

    // A selected tag-only audio row still has a real physical identity:
    // the associated CUE must not bypass its negative cue assertion.
    source.action = djmeta::FileAction::None;
    const auto tag_only = djmeta::review_batch_plan({source, cue});
    require(tag_only.decisions[1].status == djmeta::PlanStatus::Ready,
            "verified tag-only audio can still anchor an associated CUE plan");
    source.cue_links = djmeta::CueLinkState::NoExternalCue;
    const auto tag_only_conflict = djmeta::review_batch_plan({source, cue});
    require(tag_only_conflict.blocked == 1 &&
            has_reason(tag_only_conflict.decisions[1],
                "CUE_REFERENCE_PLAN_UNQUALIFIED"),
            "tag-only audio cannot silently authorize contradictory CUE rewrite");
}

void test_companion_files_need_explicit_or_qualified_policy() {
    auto a = audio("A");
    djmeta::FilePlanItem art;
    art.physical_id = "art-A";
    art.source_path = "Z:/Music/Downloads/cover.jpg";
    art.source_key = "z:/music/downloads/cover.jpg";
    art.target_path = "Z:/Music/Singles/cover.jpg";
    art.target_key = "z:/music/singles/cover.jpg";
    art.action = djmeta::FileAction::Move;
    art.role = djmeta::FileRole::Companion;
    art.associated_audio_id = a.physical_id;
    art.ruleset_revision = a.ruleset_revision;
    art.target_presence = djmeta::TargetPresence::Missing;

    const auto unapproved = djmeta::review_batch_plan({a, art});
    require(has_reason(unapproved.decisions[1], "COMPANION_NOT_AUTHORIZED"),
            "legacy moveOtherFiles must not authorize unrelated sidecars");
    art.manual_override = true;
    const auto approved = djmeta::review_batch_plan({a, art});
    require(approved.ready_to_apply, "explicitly selected artwork may be planned");
    art.manual_override = false;
    art.companion_policy_qualified = true;
    const auto by_policy = djmeta::review_batch_plan({a, art});
    require(by_policy.ready_to_apply, "qualified companion policy should be usable");
}

void test_unknown_plan_enum_values_fail_closed() {
    const auto base = audio("unknown-enum");
    auto case_item = base;
    case_item.action = static_cast<djmeta::FileAction>(42);
    auto outcome = djmeta::review_batch_plan({case_item});
    require(outcome.blocked == 1 && !outcome.ready_to_apply &&
            has_reason(outcome.decisions[0], "UNKNOWN_PLAN_ENUM_VALUE"),
            "unknown action must not become an executable file plan");

    case_item = base;
    case_item.role = static_cast<djmeta::FileRole>(42);
    outcome = djmeta::review_batch_plan({case_item});
    require(outcome.blocked == 1 && !outcome.ready_to_apply &&
            has_reason(outcome.decisions[0], "UNKNOWN_PLAN_ENUM_VALUE"),
            "unknown file role must not bypass audio/CUE qualifications");

    case_item = base;
    case_item.target_presence = static_cast<djmeta::TargetPresence>(42);
    outcome = djmeta::review_batch_plan({case_item});
    require(outcome.blocked == 1 && !outcome.ready_to_apply &&
            has_reason(outcome.decisions[0], "UNKNOWN_PLAN_ENUM_VALUE"),
            "unknown target presence must not bypass filesystem inspection");

    case_item = base;
    case_item.cue_links = static_cast<djmeta::CueLinkState>(42);
    outcome = djmeta::review_batch_plan({case_item});
    require(outcome.blocked == 1 && !outcome.ready_to_apply &&
            has_reason(outcome.decisions[0], "UNKNOWN_PLAN_ENUM_VALUE"),
            "unknown cue state must never count as qualified references");

    case_item = base;
    case_item.action = djmeta::FileAction::None;
    case_item.cue_links = static_cast<djmeta::CueLinkState>(42);
    outcome = djmeta::review_batch_plan({case_item});
    require(outcome.blocked == 1 && !outcome.ready_to_apply &&
            has_reason(outcome.decisions[0], "UNKNOWN_PLAN_ENUM_VALUE"),
            "tag-only early exit must still validate unknown enum values");

    const djmeta::BatchApproval approval{outcome.plan_fingerprint, true};
    const auto approved = djmeta::review_batch_plan({case_item}, &approval);
    require(approved.blocked == 1 && !approved.ready_to_apply,
            "overwrite approval must never bypass unknown enum values");

    // Valid examples preserve the previous successful planning contract.
    outcome = djmeta::review_batch_plan({base});
    require(outcome.ready_to_apply && outcome.blocked == 0,
            "known valid enum values must retain ready preview status");
}

void test_noop_and_missing_ids() {
    auto a = audio("A");
    a.target_key = a.source_key;
    a.target_path = a.source_path;
    a.target_presence = djmeta::TargetPresence::Unchecked;
    auto result = djmeta::review_batch_plan({a});
    require(result.ready_to_apply &&
            result.decisions[0].status == djmeta::PlanStatus::Unchanged,
            "rename/move-to-self requires no file action");

    a = audio("A");
    a.physical_id.clear();
    result = djmeta::review_batch_plan({a});
    require(has_reason(result.decisions[0], "MISSING_SOURCE_OR_RULESET_IDENTITY"),
            "missing physical identity must fail closed");

    result = djmeta::review_batch_plan({});
    require(!result.ready_to_apply, "empty plan must not be actionable");
}

} // namespace

int main() {
    test_ready_and_immutable();
    test_batch_overwrite_once_and_target_guard();
    test_large_batch_one_confirmation();
    test_missing_target_guard_and_no_inspection();
    test_new_target_after_clean_review_requires_new_consent();
    test_intra_batch_conflicts_cannot_be_overridden();
    test_stale_approval_rejects_tag_only_and_noop_rows();
    test_duplicate_subsong_source_and_self_copy();
    test_external_cue_dependencies();
    test_contradictory_audio_cue_evidence_rejected();
    test_noop_and_missing_ids();
    test_unknown_plan_enum_values_fail_closed();
    test_companion_files_need_explicit_or_qualified_policy();
    std::cout << "PASS: deterministic read-only batch plan preflight tests\n";
    return 0;
}
