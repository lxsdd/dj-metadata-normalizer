#pragma once

#include <string>
#include <vector>

namespace djmeta {

// Pure policy layer. The host is responsible for obtaining the physical key
// and source observation by READ-ONLY probes; never synthesize either from a
// display path, subsong number, or just a case-folded filename.
struct PhysicalSelectionEvidence {
    bool physical_candidate = false; // top-level subsong, no repeated host path
    std::string host_physical_key;     // volume + physical file ID or equivalent
    std::string source_guard;          // stable host-observed version evidence
};

struct PhysicalSelectionResult {
    bool qualified = false;
    std::string reason; // stable preview diagnostic, empty when qualified
};

// A repeated physical ID is disqualified for EVERY occurrence, even when the
// file was selected through two different paths (hard links / aliases).
std::vector<PhysicalSelectionResult> qualify_physical_selection(
    const std::vector<PhysicalSelectionEvidence>& selected);

} // namespace djmeta
