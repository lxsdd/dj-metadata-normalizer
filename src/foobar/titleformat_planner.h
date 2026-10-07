#pragma once

#include <SDK/foobar2000.h>

#include "djmeta/normalizer.h"

#include <string>
#include <string_view>

namespace djmeta_foobar {

// Evaluate a foobar2000 title-formatting expression against the canonical
// in-memory metadata preview. This performs no metadata or filesystem writes.
std::string evaluate_titleformat_against_canonical(
    const playable_location& location,
    const file_info& original_info,
    const djmeta::MetadataDocument& canonical_metadata,
    std::string_view expression);

} // namespace djmeta_foobar
