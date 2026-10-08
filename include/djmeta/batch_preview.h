#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace djmeta {

// The table explicitly displays unqualified operations; raw Title Formatting
// paths are NOT proof of a valid final foobar File Operations destination.
struct BatchPreviewInputRow {
    std::string source_path;
    std::string physical_id;
    std::string profile;
    std::string destination_root;
    std::string raw_relative_path;
    bool physical_source_qualified = false;
    bool semantic_proposals_pending = false;
    bool cue_dependencies_checked = false;
    bool filesystem_target_checked = false;
};

struct BatchPreviewRow {
    std::string raw_destination;
    std::vector<std::string> issues; // ordered; first is most urgent
    bool has_raw_target_collision = false;
};

struct BatchPreviewTable {
    std::vector<BatchPreviewRow> rows;
    std::size_t raw_collision_items = 0;
    std::size_t unqualified_rows = 0;
};

// Pure read-only table description; no filesystem I/O, no OS path identity
// assumptions, no use of this result as an executor approval token.
BatchPreviewTable describe_batch_preview(
    const std::vector<BatchPreviewInputRow>& items);

} // namespace djmeta
