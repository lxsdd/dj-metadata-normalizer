#pragma once

#include "djmeta/batch_plan.h"

#include <cstddef>
#include <string>
#include <vector>

namespace djmeta {

// UI-provided intent. The core does not parse foobar Title Formatting,
// resolve destination paths, inspect existing files, or persist preferences.
enum class RoutingScope { AllAudio, SelectedAudio };

struct RoutingOverride {
    RoutingScope scope = RoutingScope::SelectedAudio;
    std::vector<std::string> selected_physical_ids;
    std::string routing_profile;
    std::string naming_expression;
    FileAction action = FileAction::Move;
};

struct RoutingOverrideResult {
    bool accepted = false;
    std::size_t changed_audio_count = 0;
    std::vector<std::string> diagnostics;
    std::vector<FilePlanItem> items;
};

// Atomic in-memory application of the user's per-item or whole-batch route.
// Any changed target requires a new host Title Formatting evaluation and
// destination preflight. Related CUE/sidecar plans are invalidated too.
// No I/O and no implicit filesystem overwrite approval.
RoutingOverrideResult apply_routing_override(
    const std::vector<FilePlanItem>& original,
    const RoutingOverride& change);

} // namespace djmeta
