# Masstagger and File Operations migration inventory

Status: **SOURCE_RECEIVED — SEMANTIC MIGRATION NOT YET QUALIFIED**

The goal is to replace the user's actual historical foobar2000 workflow, while
reviewing each old action for correctness. An existing Masstagger expression is
evidence of user intent, **not automatically a new default rule**.

## Source snapshot

On 2026-10-08 the user provided seven Masstagger export files:

1. `discogs - continous mix-cd single artist.mts`
2. `discogs - continous mix-cd various artists.mts`
3. `discogs - cue file ohne remix tag.mts`
4. `discogs - single track mit remix tag.mts`
5. `replace-scripts.mts`
6. `single track mit remix tack ohne quelle.mts`
7. `deezer - single track mit remix-tag.mts`

These original binary uploads are part of the conversation evidence, **not**
assumed to reside in this source repository. The initial user-facing audit
reported 143 action occurrences and 47 distinct action parameters; a
reproducible full normalized action inventory and golden tests are still
required before introducing any semantic rules.

The user separately supplied two literal foobar File Operations exports,
preserved as historical, **disabled reference fixtures**:

- `fixtures/legacy-fileops/FileOps-Presets.txt`:
  `Singles`, `Alben`, `Livesets` move presets;
- `fixtures/legacy-fileops/FileOps-Presets-Rename.txt`:
  `Cue-Files`, `Alben`, `Singles` rename presets.

These fixture files describe existing behavior; loading this repository must
not automatically activate the old `overwrite=yes` policies or issue
filesystem operations.

## Findings and preservation rules

- Old scripts reuse long substitution chains. Extract shared rule primitives
  and stable aliases, do not duplicate them across source profiles.
- Case-changing or transliteration substitutions, such as DJ/Dj or
  Tiësto/Tiesto, can change proper names and therefore must not be SAFE.
- Blanket field whitelists can destroy metadata not known to a script, such
  as ISRC and third-party tags. Unknown metadata is preserved by default.
- TITLE/MIX/REMIX parsing through parenthesis positions is not sufficiently
  reliable to apply silently to all files; source-dependent interpretation
  starts as REVIEW.
- `DATE` is the user's usual four-digit year tag and `DATE_RAW` preserves
  full/original dates. Both remain independently addressable and user-
  configurable; no automatic truncation of the only complete date.
- The move presets use `moveOtherFiles=yes`, `removeEmpty=yes`, and
  `overwrite=yes`. Preserve the user's ability to do bulk overwrite after
  **one deliberate reviewed approval**, but not an unconditional overwrite.
- External CUE files are dependencies with potentially changing `FILE`
  references; rename/update behavior is separate from embedded cuesheets.
- Single/Album/Liveset routing can be auto-suggested, but the user may
  override naming, destination and action for an item or selected batch
  before Apply. A changed override invalidates prior consent.

## Mapping and test workflow

`source receipt → action-level decode/inventory → overlap and loss audit →
proposed v3+ semantic primitive(s) → representative golden fixtures →
impact preview → user decision for new semantic changes → enablement`.

The current `rules/default-rules.json` **must not** receive historical
substitutions merely because the original Masstagger scripts contain them.

| Source | Scope | State |
| --- | --- | --- |
| Seven *.mts files | Tag cleanup, field mapping, title/version parsing | RECEIVED / REVIEW REQUIRED |
| Move presets | Three host-native Title Formatting routes | REFERENCE CAPTURED |
| Rename presets | Three host-native Title Formatting expressions | REFERENCE CAPTURED |
| Cue/sidecars | Exact external references and dependencies | SAFETY CONTRACT FIXED, IMPLEMENTATION PENDING |
| Batch overwrite | One approval bound to exact reviewed plan | PURE PREFLIGHT ENGINE, NO EXECUTOR |

The legacy foobar File Operations menu and Title Formatting semantics remain
the authoritative runtime reference; the portable core does not attempt to
implement foobar's `%field%` or `$if()` interpreter.
