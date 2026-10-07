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
- ordered per-rule change trace;
- field + multivalue index;
- raw original value;
- value immediately before the rule;
- proposed value;
- rule ID;
- SAFE / CONFIDENT / REVIEW;
- rationale;
- complete canonical preview document.

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

The C++ engine currently exposes the typed rule model directly. Loading the persisted JSON ruleset into that model is the next core milestone; the JSON contract is already CI-validated so adapters do not invent their own schema.

## Shared-engine integration

The foobar component links the C++ core directly.

DJ Library must not port rules into C#. The intended integration is a small versioned C ABI wrapper built from this repository and called from C# via P/Invoke. The ABI transports UTF-8 metadata/rule input and preview output; ownership/version functions will be explicit.

This keeps behavior identical across both surfaces while avoiding C++ ABI coupling to .NET.

## Write boundary

The future foobar writer is an adapter, not part of the core. It will use supported foobar2000 SDK metadata update APIs and mutate only explicitly approved fields.

No direct audio-file editing is permitted in DJ Library or in the Bridge.

Virtual subsongs require a separate qualification gate because writes must not erase physical metadata that is absent from a projected subsong view.
