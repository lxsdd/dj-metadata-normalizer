# Bridge integration contract

## Existing v2 finding

`foo_dj_library_bridge` schema v2 already provides the correct single-snapshot transport and exact `(path, subsong)` identity.

Its `extra_metadata_json` preserves true vectors only for metadata outside the 24-field core projection. Core metadata names are intentionally excluded from that generic JSON object, while the core projection joins multivalue fields with `; `.

That is sufficient for current DJ Library display/matching behavior but is not lossless enough for normalization. For example, these inputs become indistinguishable in a flattened column:

- `ARTIST = ["A", "B"]`
- `ARTIST = ["A; B"]`

A normalizer must not guess which physical metadata structure existed.

## Minimal compatible extension

Do **not** create another database, sidecar export or IPC channel.

Proposed Bridge schema v3 appends one column to the existing payload:

`metadata_vectors_json`

It contains every metadata field as exact ordered value vectors, including core fields. The existing first 25 schema-v2 columns remain byte-compatible in meaning and order.

Example:

```json
[
  {"name":"ARTIST","values":["A","B"]},
  {"name":"GENRE","values":["House","Tech House"]},
  {"name":"TITLE","values":["Track"]}
]
```

The array-of-entry shape is deliberate: it preserves field/value structure without relying on JSON object-key uniqueness.

Requirements:

- deterministic field ordering;
- exact value ordering and duplicate preservation;
- JSON escaping;
- no synthetic aliases;
- no collapsing `DATE` into `YEAR`, `LABEL` into `PUBLISHER`, etc.;
- include the exact vector JSON in `tag_fingerprint`;
- preserve profile-local/multi-instance behavior and atomic completeness protocol;
- Bridge remains read-only toward audio files/tags/private databases.

DJ Library may continue ignoring the appended column until its normalizer integration is implemented.

## Why not reuse extra_metadata_json alone?

Because schema v2 explicitly excludes the core names from `extra_metadata_json`. Changing that meaning in-place would create two representations of the same core metadata and would make v2 consumers ambiguous. A new appended v3 column is clearer and backward-compatible at the row-prefix level.

## Qualification

Before v3 can be consumed by the normalizer:

- Bridge contract tests must prove vector ordering, duplicates, aliases and escaping;
- Win32/x64 builds must remain green;
- existing DJ Library v1/v2 compatibility tests must remain green;
- an added DJ Library v3 fixture must prove the extra column is safely ignored by legacy product behavior;
- runtime snapshot counts/identity behavior must remain unchanged.
