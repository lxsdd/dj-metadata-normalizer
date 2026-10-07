# Repository operating rules

This repository is the canonical source of truth for DJ Metadata Normalizer.

## Scope isolation

- Keep this project fully separate from `rekordbox-mytag-sync`: no shared branches, issues, release lifecycle or write path.
- `foo_dj_library_bridge` remains a read-only integration/data source.
- `dj-library` remains a separate product repository and must consume the shared normalizer contract rather than reimplement normalization rules.

## Safety contract

- Analysis and writes are different operations.
- The core is pure analysis: immutable metadata in, preview proposals out.
- Original values and canonical preview values must remain separately observable.
- No background writes.
- Only an explicit foobar2000 UI action may later invoke a write adapter.
- A writer must reject stale previews if the input metadata or ruleset revision changed.
- New semantic rules start as REVIEW unless specifically qualified otherwise.
- SAFE is reserved for transformations proven deterministic and non-semantic.

## Engineering

- C++20 core, deterministic behavior on Win32/x64 and CI hosts.
- GitHub Actions is the build authority.
- Tests must cover rule ordering, multivalue preservation, safety classification, immutable input behavior and fingerprints.
- Do not add a second snapshot/export channel when the existing Bridge snapshot can be extended compatibly.
- Any change to the Bridge must preserve exact `(path, subsong)` identity, profile-local behavior and read-only audio/tag semantics.

## Development order

1. core model + preview engine;
2. versioned ruleset contract + deterministic tests;
3. Bridge metadata-vector compatibility, only as required;
4. foobar preview UI;
5. DJ Library review integration using the same engine/rules;
6. foobar write adapter only after stale-preview and regression gates are qualified.
