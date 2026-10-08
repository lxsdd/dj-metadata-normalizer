#pragma once

// Pure read-only provenance guard: a revision string alone is insufficient.
// Users or other applications can replace ruleset.json without bumping its
// revision. The exact UTF-8 bytes and resolved rules source are part of every
// open preview's captured provenance.
#include <stdexcept>
#include <string>
#include <string_view>

namespace djmeta {

struct RulesTextSnapshot {
    std::string json;
    std::string source_label;
};

inline bool rules_snapshot_matches(const RulesTextSnapshot& captured,
                                   std::string_view current_json,
                                   std::string_view current_source_label) {
    return captured.json == current_json &&
           captured.source_label == current_source_label;
}

inline void require_rules_snapshot(const RulesTextSnapshot& captured,
                                   std::string_view current_json,
                                   std::string_view current_source_label) {
    if (!rules_snapshot_matches(captured, current_json, current_source_label))
        throw std::invalid_argument(
            "Shared normalization rules changed while this preview was open. "
            "Close and reopen Prepare Tracks to analyze with the current rules.");
}

} // namespace djmeta
