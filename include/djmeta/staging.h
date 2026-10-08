#pragma once

#include "djmeta/normalizer.h"

#include <cstddef>

namespace djmeta {

struct StagedMetadata {
    MetadataDocument document;
    std::size_t safe_proposals_applied = 0;
    std::size_t unresolved_proposals = 0;
};

// Construct a read-only planning input from raw tags. A proposal that has
// ANY CONFIDENT/REVIEW provenance is NOT selected automatically. This is
// intentionally conservative for chains of SAFE + semantic transformations.
StagedMetadata stage_safe_only(
    const MetadataDocument& original,
    const AnalysisResult& analysis);

} // namespace djmeta
