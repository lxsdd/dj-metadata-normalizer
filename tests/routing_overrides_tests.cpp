#include "djmeta/routing_overrides.h"
#include "djmeta/normalizer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}
djmeta::FilePlanItem audio(const char* id) {
    djmeta::FilePlanItem item;
    item.physical_id = id;
    item.source_path = std::string("Z:/music/downloads/") + id + ".flac";
    item.source_key = item.source_path;
    item.target_path = std::string("Z:/music/singles/") + id + ".flac";
    item.target_key = item.target_path;
    item.target_presence = djmeta::TargetPresence::Existing;
    item.target_guard = "observed-target-content";
    item.routing_profile = "Singles";
    item.naming_expression = "%artist% - %title%";
    item.action = djmeta::FileAction::Move;
    item.metadata_fingerprint = "raw";
    item.planned_metadata_fingerprint = "canonical";
    item.ruleset_revision = "2026-10-07.2";
    item.cue_links = djmeta::CueLinkState::Verified;
    return item;
}
void test_selection_is_atomic_and_recomputes() {
    auto a = audio("A"), b = audio("B");
    djmeta::FilePlanItem cue;
    cue.physical_id = "cue-A";
    cue.associated_audio_id = "A";
    cue.role = djmeta::FileRole::ExternalCue;
    cue.source_path = "Z:/music/downloads/A.cue";
    cue.source_key = cue.source_path;
    cue.target_path = "Z:/music/singles/A.cue";
    cue.target_key = cue.target_path;
    cue.target_presence = djmeta::TargetPresence::Existing;
    cue.target_guard = "old-cue";
    cue.cue_links = djmeta::CueLinkState::Verified;
    cue.cue_postimage_fingerprint = "cue-old-postimage";
    cue.cue_references_will_change = true;
    cue.action = djmeta::FileAction::Move;
    std::vector<djmeta::FilePlanItem> batch = {a,b,cue};
    const auto raw = batch;
    const auto fingerprint = djmeta::review_batch_plan(batch).plan_fingerprint;
    const djmeta::RoutingOverride choice{
        djmeta::RoutingScope::SelectedAudio, {"A"}, "Alben",
        "%tracknumber%. %artist% - %title%", djmeta::FileAction::Move
    };
    const auto result = djmeta::apply_routing_override(batch, choice);
    require(result.accepted && result.changed_audio_count == 1, "single override should be accepted");
    require(result.items.size() == 3 && result.items[0].routing_profile == "Alben",
            "selected route not changed");
    require(result.items[0].manual_override, "manual source not marked");
    require(result.items[0].target_path.empty() && result.items[0].target_key.empty() &&
            result.items[0].target_presence == djmeta::TargetPresence::Unchecked &&
            result.items[0].target_guard.empty() &&
            result.items[0].cue_links == djmeta::CueLinkState::Unchecked,
            "selected source preflight was not invalidated");
    require(result.items[1].routing_profile == "Singles" &&
            result.items[1].target_path == b.target_path,
            "unselected source was rewritten");
    require(result.items[2].cue_postimage_fingerprint.empty() &&
            !result.items[2].cue_references_will_change &&
            result.items[2].target_path.empty(),
            "related external cue plan was not invalidated");
    require(batch[0].target_path == raw[0].target_path &&
            batch[2].cue_postimage_fingerprint == raw[2].cue_postimage_fingerprint,
            "original snapshot mutated");
    require(djmeta::review_batch_plan(result.items).plan_fingerprint != fingerprint,
            "approved plan fingerprint not invalidated");
}
void test_bulk_override() {
    std::vector<djmeta::FilePlanItem> batch = {audio("A"), audio("B")};
    const djmeta::RoutingOverride change{
        djmeta::RoutingScope::AllAudio, {}, "Livesets",
        "%artist%\\%artist% - %album%", djmeta::FileAction::Copy
    };
    const auto result = djmeta::apply_routing_override(batch, change);
    require(result.accepted && result.changed_audio_count == 2,
            "bulk route action did not affect all audio");
    for (const auto& item : result.items) {
        require(item.routing_profile == "Livesets" &&
                item.action == djmeta::FileAction::Copy &&
                item.manual_override && item.target_path.empty() &&
                item.target_presence == djmeta::TargetPresence::Unchecked,
                "bulk source should require host preflight");
    }
}
void test_fail_closed_on_missing_or_duplicate_ids() {
    auto item = audio("A");
    const std::vector<djmeta::FilePlanItem> batch = {item};
    djmeta::RoutingOverride miss{djmeta::RoutingScope::SelectedAudio, {"missing"}, "Alben", "%title%", djmeta::FileAction::Move};
    require(!djmeta::apply_routing_override(batch, miss).accepted, "unknown ID accepted");
    miss.selected_physical_ids = {"A","A"};
    require(!djmeta::apply_routing_override(batch, miss).accepted, "duplicate selected IDs accepted");
    miss.selected_physical_ids.clear();
    require(!djmeta::apply_routing_override(batch, miss).accepted, "empty selected set accepted");
    miss.selected_physical_ids = {"A"};
    miss.naming_expression.clear();
    require(!djmeta::apply_routing_override(batch, miss).accepted, "missing expression accepted");
    miss.naming_expression = "%title%\nDelete";
    require(!djmeta::apply_routing_override(batch, miss).accepted, "multiline expression accepted");
    miss.naming_expression = "%title%";
    const auto dup = std::vector<djmeta::FilePlanItem>{item,item};
    require(!djmeta::apply_routing_override(dup, miss).accepted,
            "two items with same physical ID accepted");
    const djmeta::RoutingOverride invalid_bulk{
        djmeta::RoutingScope::AllAudio, {"A"}, "Alben", "%title%", djmeta::FileAction::Move};
    require(!djmeta::apply_routing_override(batch, invalid_bulk).accepted,
            "nonempty selected IDs in all-audio scope accepted");
}
void test_manual_none_keeps_tags_and_invalidates_plan() {
    const auto a = audio("A");
    const djmeta::RoutingOverride choice{
        djmeta::RoutingScope::SelectedAudio, {"A"}, "",
        "", djmeta::FileAction::None
    };
    const auto result = djmeta::apply_routing_override({a},choice);
    require(result.accepted && result.items.size()==1 &&
            result.items[0].action == djmeta::FileAction::None &&
            result.items[0].metadata_fingerprint == a.metadata_fingerprint &&
            result.items[0].planned_metadata_fingerprint == a.planned_metadata_fingerprint,
            "tag-only selection should preserve metadata plan");
}
}

int main() {
    test_selection_is_atomic_and_recomputes();
    test_bulk_override();
    test_fail_closed_on_missing_or_duplicate_ids();
    test_manual_none_keeps_tags_and_invalidates_plan();
    std::cout << "PASS: route overrides and invalidation (4 suites)\n";
}
