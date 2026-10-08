#pragma once

#include <SDK/foobar2000.h>

#include <cstddef>

namespace djmeta_foobar {

// Initial read-only route preview for a manually selected legacy profile.
// This does not determine which profile fits a track, inspect any target,
// approve metadata proposals, or authorize any filesystem action.
void show_legacy_route_preview(
    const metadb_handle_list& handles,
    std::size_t route_index);

} // namespace djmeta_foobar
