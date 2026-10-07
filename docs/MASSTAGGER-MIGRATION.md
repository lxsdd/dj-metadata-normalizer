# Masstagger migration inventory

Status: **SOURCE_REQUIRED**

The normalization project must replace the user's actual productive foobar2000 Masstagger behavior, not reconstruct plausible rules from examples or collection contents.

## Evidence currently available

Existing project evidence identifies the currently accepted productive Masstagger configuration by SHA-256:

`8B269FF6E0AAE58E3315540154520F730E88FFF939B95D29FC91FA21004DC6B1`

Historical project notes also prove that real exported/imported `.mts` actions were used during metadata-isolation qualification. Those tests prove Masstagger interoperability only; they do **not** define the user's normalization policy.

The actual productive configuration bytes and the complete exported `.mts` scripts are not present in the currently available project/repository/file sources.

## Migration rule

Until those source bytes/scripts are available:

- do not infer canonical artist, label, genre, remix, version or separator rules from library data;
- do not promote examples used in unit tests into the default ruleset;
- do not claim the Masstagger replacement is feature-complete;
- keep the default ruleset limited to independently safe technical cleanup.

## Required source capture

For deterministic migration, capture either:

1. the productive Masstagger configuration whose SHA-256 matches the accepted hash above; and/or
2. every productive normalization script exported from Masstagger as `.mts`.

When supplied, preserve the originals as immutable migration fixtures and record their SHA-256 values.

## Inventory format

Each discovered Masstagger action will be classified into this table before implementation:

| ID | Source script/action | Input field(s) | Operation | Output field(s) | Cross-field | Proposed primitive | Initial safety | Golden fixtures | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| pending | source required | - | - | - | - | - | REVIEW | - | BLOCKED_SOURCE |

The migration order is deterministic:

`source capture -> action inventory -> semantic grouping -> rule primitive mapping -> golden tests -> impact analysis -> enablement`.

No migrated semantic rule becomes SAFE solely because it existed in Masstagger.
