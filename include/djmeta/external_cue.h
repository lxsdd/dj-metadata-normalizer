#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace djmeta {

// Pure syntax inventory of a separate .cue file. No filesystem access, no
// source/target resolution, no encoding conversion and no modifications.
enum class CueSyntaxStatus { Parsed, NeedsReview, Invalid };
enum class CueTextEncoding { Utf8, Utf8Bom, UnsupportedUtf16, Unknown };

struct ExternalCueReference {
    // Exact raw UTF-8 value between FILE quotes (or the unquoted token).
    // Never infer actual audio identity from this string alone.
    std::string filename;
    std::string file_type;
    std::size_t line_number = 0;  // one-based
    std::size_t filename_begin = 0; // raw byte offset, inclusive
    std::size_t filename_end = 0;   // raw byte offset, exclusive
    bool quoted = false;
};

struct ExternalCueInventory {
    CueSyntaxStatus status = CueSyntaxStatus::Invalid;
    CueTextEncoding encoding = CueTextEncoding::Unknown;
    std::vector<ExternalCueReference> references;
    std::vector<std::string> diagnostics;
    bool multiple_files = false;
    std::size_t source_bytes = 0;
};

// "Parsed" means only that FILE syntax was recognized under strict UTF-8.
// It does NOT imply that the FILE references exist or that a rename/move is
// safe. Those decisions require foobar's source/target path and identity
// resolution, exact metadata/CUE fingerprints and the batch preflight gate.
ExternalCueInventory inspect_external_cue(std::string_view raw_bytes);

} // namespace djmeta
