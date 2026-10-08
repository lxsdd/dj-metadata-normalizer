#pragma once

#include <SDK/foobar2000.h>

namespace djmeta_foobar {
struct RoutePreviewChoice;

// Interactive but read-only: row selection + per-selected/all route overrides.
// All source/target identity and CUE dependency checks remain unqualified.
// No file actions, metadata writes, or persistent configuration changes.
void show_batch_preview_dialog(
    const metadb_handle_list& handles,
    const RoutePreviewChoice& initial_choice);

} // namespace djmeta_foobar
