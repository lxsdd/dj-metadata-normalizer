#pragma once

#include "djmeta/normalizer.h"

#include <string_view>

namespace djmeta {

// This is a PURE comparison of *physical textual metadata* snapshots only.
// It is not a tag-writer or source-identity proof. ReplayGain, embedded CUE,
// binary artwork and technical fields require separate comparisons.
enum class TagWriteDecision {
    Unchanged,     // exact multivalue payload is semantically equivalent
    Changed,       // one or more text-tag names / values differ
    Unqualified    // duplicate/invalid field structure; no write permitted
};

// foobar textual field names are ASCII case-insensitive and metadata field
// enumeration order has no semantic meaning; each field's value sequence DOES.
// Do not rewrite a file to reorder fields or change only tag-key letter case.
// No trim, Unicode normalization, filename comparison or implicit tag deletion.
TagWriteDecision compare_physical_text_tags(
    const MetadataDocument& original,
    const MetadataDocument& approved_postimage);

// Pure byte-level CUE/sidecar guard: NOT a normalized-text comparison.
// Equal source/postimage bytes must never be passed to a file writer.
// Caller is responsible for current physical source and encoding qualification.
bool cue_bytes_rewrite_needed(std::string_view observed_source_bytes,
                              std::string_view approved_postimage_bytes);

} // namespace djmeta
