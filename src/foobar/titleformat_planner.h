#pragma once

#include <SDK/foobar2000.h>

#include "djmeta/normalizer.h"

#include <map>
#include <string>
#include <string_view>

namespace djmeta_foobar {

// Lifetime-bound cache for compiling a host titleformat expression once per
// preview/edit transaction, not once per selected track. Never global:
// invalid input or user-edited expressions must not carry stale compiled
// references into another preview. All evaluation still uses foobar SDK.
class TitleformatBatchEvaluator {
public:
    std::string evaluate(const playable_location& location,
                         const file_info& original_info,
                         const djmeta::MetadataDocument& canonical_metadata,
                         std::string_view expression);
private:
    std::map<std::string, titleformat_object::ptr> compiled_;
};

// Evaluate a foobar2000 title-formatting expression against the canonical
// in-memory metadata preview. This performs no metadata or filesystem writes.
std::string evaluate_titleformat_against_canonical(
    const playable_location& location,
    const file_info& original_info,
    const djmeta::MetadataDocument& canonical_metadata,
    std::string_view expression);

} // namespace djmeta_foobar
