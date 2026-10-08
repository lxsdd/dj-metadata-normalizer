#pragma once

#include "djmeta/normalizer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace djmeta {

// Stable, read-only view of the actual rule proposals, NOT the raw
// canonical_preview (which can contain unapproved REVIEW chains).
struct MetadataDiffRow {
    std::size_t source_index = 0;
    std::size_t field_index = 0;
    std::size_t value_index = 0;
    std::string field;
    std::string original;
    std::string proposed;
    SafetyClass safety = SafetyClass::Review;
    std::string rule_ids;
    std::string rationales;
    std::size_t proposal_index = 0; // stable index within analysis.proposals
};

std::vector<MetadataDiffRow> describe_metadata_diffs(
    const std::vector<AnalysisResult>& analyses);

std::vector<std::size_t> sort_metadata_diff_rows(
    const std::vector<MetadataDiffRow>& rows,
    const std::vector<std::string>& source_labels,
    int sort_column, bool descending);

} // namespace djmeta
