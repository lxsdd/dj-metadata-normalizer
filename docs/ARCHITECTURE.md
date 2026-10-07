# Architecture

## Product boundary

DJ Metadata Normalizer is a separate product from Rekordbox MyTag Sync.

The architecture is deliberately split into four layers:

1. **normalizer core** — pure deterministic analysis;
2. **foobar adapter** — selection, preview UI and later explicit SDK-based writes;
3. **DJ Library adapter** — review and rule management over the same core;
4. **Bridge integration** — read-only metadata snapshots from `foo_dj_library_bridge`.

There is one normalization implementation and one versioned rule model.

## Core contract

Input is an immutable ordered metadata document. Each field retains its original name and its vector of values; multivalue metadata is never flattened by the core.

Output contains:

- SHA-256 input fingerprint;
- ruleset revision;
- an ordered low-level per-rule change trace;
- aggregated user-facing final proposals;
- exact field index + field name + multivalue index;
- raw original value;
- value immediately before each rule;
- final proposed value;
- ordered rule IDs and rationales;
- aggregated SAFE / CONFIDENT / REVIEW classification;
- complete canonical preview document.

The separate field index is intentional: duplicate same-named metadata entries remain structurally distinguishable.

Rules execute by ascending priority and then stable rule ID. A later rule sees the preview result of earlier rules, while every trace entry retains the raw original value.

Analysis performs no I/O and cannot write tags.

## Safety model

### SAFE

Reserved for transformations demonstrated to be deterministic and non-semantic. The initial allow-list is intentionally tiny: leading/trailing ASCII whitespace removal and repeated ASCII whitespace collapse.

### CONFIDENT

Strong canonicalization with domain meaning. It remains visible and selected only according to the future review UI policy.

### REVIEW

Ambiguous, semantic, newly learned or not-yet-qualified transformations. New rules derived from user corrections begin here.

The v1 rules validator fails closed if a generic replacement rule is marked SAFE.

## Stale-preview contract

Every analysis is bound to both:

- `input_fingerprint` — SHA-256 over a length-prefixed exact metadata representation;
- `ruleset_revision`.

Before a future write, the foobar adapter must re-read the selected item's metadata and re-evaluate both conditions. A mismatch invalidates the preview and requires re-analysis. The writer must never treat a stale preview as authority.

`(path, subsong)` identifies the track; it does not replace the metadata fingerprint.

## Rule model v1

The canonical persisted representation is `rules/schema-v1.json` + a concrete ruleset such as `rules/default-rules.json`.

v1 deliberately starts with a small deterministic transformation vocabulary:

- match always;
- match exact value;
- trim ASCII whitespace;
- collapse ASCII whitespace;
- replace with a canonical value.

The schema is designed to evolve with explicit versions. Unicode normalization, artist collaboration parsing, title/version extraction, genre logic, aliases and cross-field rules are added as named, testable primitives rather than an opaque scripting language.

The C++ engine owns the strict schema-v1 JSON loader. The persisted ruleset is therefore parsed and validated by the same code used by both integration surfaces; adapters do not implement their own rule parser.

## Shared-engine integration

The foobar component links the C++ core directly.

DJ Library must not port rules into C#. A versioned analysis-only C ABI is implemented in this repository for C#/P/Invoke integration. It accepts UTF-8 Bridge-v3 `metadata_vectors_json` plus the canonical ruleset JSON and returns deterministic preview JSON. ABI-owned strings have an explicit release function and exceptions never cross the ABI.

The canonical active runtime rules path is `%APPDATA%\DJMetadataNormalizer\ruleset.json`; the foobar preview opens it read-only and falls back to the embedded versioned default ruleset when absent.

This keeps behavior identical across both surfaces while avoiding C++ ABI coupling to .NET.

## Write boundary

The future foobar writer is an adapter, not part of the core. It will use supported foobar2000 SDK metadata update APIs and mutate only explicitly approved fields.

No direct audio-file editing is permitted in DJ Library or in the Bridge.

The first foobar integration milestone is deliberately analysis-only. A static CI audit rejects write-capable SDK/file tokens before Win32/x64 component builds run.

Virtual subsongs require a separate qualification gate because writes must not erase physical metadata that is absent from a projected subsong view.
