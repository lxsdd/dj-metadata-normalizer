#pragma once

#include <SDK/foobar2000.h>

#include <cstddef>
#include <string>

namespace djmeta_foobar {

// Initial read-only route preview for a manually selected legacy profile.
// This does not determine which profile fits a track, inspect any target,
// approve metadata proposals, or authorize any filesystem action.
struct RoutePreviewChoice {
    std::string display_name;
    std::string destination_root;
    std::string titleformat_expression;
};

// Read-only preview against a user-edited profile. Does not persist the
// choice or execute any file operation.
void show_custom_route_preview(
    const metadb_handle_list& handles,
    const RoutePreviewChoice& choice);

void show_legacy_route_preview(
    const metadb_handle_list& handles,
    std::size_t route_index);

} // namespace djmeta_foobar
