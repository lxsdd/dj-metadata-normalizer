# External CUE inventory and reference rewrite preview

Status: **IMPLEMENTED AS PURE CORE API; NO HOST INTEGRATION OR WRITER**

This code was built in a separate feature branch from unmerged Batch Planner PR #6.
It has no filesystem operations and does not modify any user files.

## Read-only inventory

`djmeta::inspect_external_cue(raw_bytes)` inventories external `.cue`
`FILE` directives, including:

- exact original text bytes and reference filename byte spans;
- source line numbers and quoted / unquoted forms;
- single- or multiple-`FILE` lists;
- accepted explicit UTF-8 with/without BOM;
- detection of malformed FILE syntax, invalid UTF-8, NUL data,
  unsupported UTF-16 and ambiguous legacy encodings;
- REVIEW classification for absolute file references or unrecognized
  file types requiring further host qualification.

`CueSyntaxStatus::Parsed` only means the FILE syntax was recognized; it
does not mean the referenced audio files exist, are the expected physical
files or can be renamed safely.

The inventory intentionally does **not** infer audio identity from a
matching basename and does not conflate external cues with embedded
cuesheets.

## In-memory reference rewrite proposal

`djmeta::preview_external_cue_reference_rewrite(raw_bytes, renames)`
requires explicit mappings of reference index, exact expected old filename
and proposed new filename.

The function:

- reparses the exact supplied original bytes before preparing edits;
- rejects stale expected references, duplicate reference mappings and
  filenames containing quotes, controls or invalid UTF-8;
- modifies only specifically selected `FILE` filename spans;
- adds quotes if a previously unquoted path gains whitespace;
- preserves all other bytes, including BOM, newline convention,
  timing/INDEX/TRACK, REM fields and other referenced audio files;
- reparses the in-memory postimage before returning an eligible result;
- never writes the postimage to a disk or to an embedded cuesheet.

`eligible=true` only means the text transformation is syntactically
valid. It is **not** execution approval. The host must still validate
source identity, reference resolution, destination collision safety, the
source CUE byte fingerprint and approved whole-batch plan.

## What remains unqualified

- Real-world user CUE samples and a broad encoding matrix, including
  Windows-1252, CP932, CP1251 and UTF-16 decoding/roundtrip;
- foobar host path resolution and exact per-reference audio mapping;
- companion file discovery and routing UI;
- cue filename changes themselves (separate from FILE references);
- physical-vs-virtual/embedded cue metadata writes;
- per-item and bulk approval integration in foobar UI;
- tag writes, move/copy/rename execution and host timestamp policy.

Encoding handling will remain extensible through explicit qualified
decoders. Unknown byte sequences must not be silently converted.

## Local qualification without GitHub Actions minutes

The pure component can be compiled separately with C++20 and
`-Wall -Wextra -Wpedantic -Werror` on Linux. Independent local tests
cover seven suites for UTF-8/BOM, byte spans, single/multi-FILE,
syntax failure, stale reference refusal and exact in-memory postimage.
Additional sanitizers and malformed-input stress tests are used during
development but do not replace later MSVC Win32/x64 and foobar host CI.

This branch is **not** a qualified release and must not be merged into
`main` until exact-head CI gates and host adapter dependencies pass.

## Host-qualified Cue association planning

The next portable core API `qualify_external_cue_association()` combines
the strict external-CUE inventory with **host-verified physical audio
identities**. It cannot infer identities from matching basenames.

The foobar adapter must supply, for **every FILE reference**, its exact
original filename, an unambiguous canonical physical source key, the
planned destination-relative reference filename, and proof that the
reference resolves to that **same physical file** after the planned
audio/CUE operations.

The planner:

- rejects missing, duplicated, stale or ambiguous reference mappings;
- requires full coverage for multi-FILE CUEs, including references to
  audio files *outside* the selected batch;
- maps selected audio to Cue references via host-canonical source identity,
  rejecting duplicate selected physical IDs and source identities;
- produces exact in-memory Cue reference postimage and SHA-256 original and
  postimage fingerprints for the batch approval and stale-state gate;
- preserves unselected FILE references when verified safe after the move;
- returns only an **advisory** qualification: the host executor must re-probe
  and revalidate all physical paths and source bytes before any write.

The portable core does not compute platform-specific relative paths, probe
the media library or filesystem, or impersonate foobar's Title Formatting
and File Operations behavior. Read-only local Clang ASan/UBSan tests cover
five association test suites and 20,000 randomized raw Cue inputs.

**Remaining adapter work:** bind foobar physical source/path services to
these inputs; evaluate destination-relative FILE expressions; display
routing and filename choices; validate CUE/sidecar relations against live
sources; qualify Win32/x64. No writer is enabled.
