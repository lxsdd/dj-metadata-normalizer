#pragma once

#include "djmeta/metadata_diff.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace djmeta {

enum class MetadataFocus { Music, Extended, All };

struct TrackReviewSummary {
    std::size_t source_index = 0;
    std::size_t music_changes = 0;
    std::size_t extended_changes = 0;
    std::size_t review_required = 0;
    std::size_t safe_changes = 0;
    std::size_t confident_changes = 0;
};

// A view classification ONLY: every proposal and its provenance are retained.
// Custom/unknown fields remain visible under Extended / All, never deleted.
bool is_music_metadata_field(std::string_view name);
std::vector<TrackReviewSummary> summarize_track_changes(
    std::size_t track_count,
    const std::vector<MetadataDiffRow>& proposals);

std::vector<MetadataDiffRow> selected_track_diffs(
    const std::vector<MetadataDiffRow>& proposals,
    std::size_t source_index, MetadataFocus focus);

// Sorting changes presentation only; source index identity remains stable.
std::vector<std::size_t> sort_track_summaries(
    const std::vector<TrackReviewSummary>& summaries,
    const std::vector<std::string>& display_labels,
    int column, bool descending);

} // namespace djmeta
