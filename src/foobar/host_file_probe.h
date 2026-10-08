#pragma once

#include <string>

namespace djmeta_foobar {

// Non-mutating OS/foobar filesystem observation. This is intentionally
// separate from the portable normalizer and never establishes approval for a
// tag writer, File Operations, CUE rewrite, or final destination naming.
enum class HostFileState { ExistingFile, Missing, Unqualified };

struct HostFileObservation {
    HostFileState state = HostFileState::Unqualified;
    std::string canonical_path;
    std::string host_physical_key; // physical ID, not a normalized path guess
    std::string source_guard;      // physical ID + observed size/times/attributes
    std::string detail;            // human-readable reason if unqualified
};

// Missing is a legitimate observation for a proposed target but NOT for a
// selected source. The caller enforces that distinction.
// No files are opened for writing, created, moved or modified.
HostFileObservation probe_host_file_readonly(const std::string& path);

} // namespace djmeta_foobar
