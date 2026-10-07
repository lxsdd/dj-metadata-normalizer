# Unified Prepare-Track Workflow

## Product goal

DJ Metadata Normalizer replaces the current multi-step foobar2000 workflow with one explicit, reviewable plan:

1. analyze and normalize metadata;
2. derive the final filename from the canonical metadata;
3. derive the destination path and file action from the same canonical metadata;
4. preflight the complete batch;
5. show one unified preview;
6. apply only after explicit approval.

The shared engine computes desired state. It does not write tags or touch the filesystem.

## Host Policy Inheritance

foobar2000 remains the authority for host behavior wherever the host already defines policy.

The foobar component must therefore prefer supported foobar2000 SDK services and conventions for:

- metadata updates;
- title formatting;
- filename/path formatting behavior;
- rename/copy/move execution;
- timestamp behavior associated with host operations;
- progress/cancel integration;
- library/database notifications;
- Preferences, Apply/Cancel/Reset behavior;
- Dark Mode and DPI behavior;
- persistent component configuration.

The plugin must not create parallel settings for host behavior merely because it can.

If a required foobar policy is not exposed by the public SDK, the component may add a compatibility adapter only after that gap is documented and qualified. Such an adapter should emulate host behavior rather than invent a second user-facing policy.

## Configuration conventions

foobar-specific state belongs to foobar's normal configuration mechanisms and Preferences UI.

Rules:

- stable configuration GUIDs;
- cfg-backed persistence or the current SDK-equivalent host configuration service;
- Preferences changes are staged until Apply/OK;
- Cancel must not persist staged changes;
- Reset restores one centralized source of defaults;
- visible controls and HasChanged/apply logic use the same normalized readers;
- obsolete hidden settings are not deleted blindly when profile compatibility depends on their GUIDs;
- Dark Mode support is advertised and implemented through host hooks;
- DPI and keyboard/accessibility behavior follow native foobar conventions.

These conventions intentionally reuse lessons already qualified in `foo_smart_tempo` and `foo_metadata_isolation`; those repositories remain separate products and are not runtime dependencies.

The one intentional cross-surface exception is the shared versioned normalizer ruleset:

`%APPDATA%\DJMetadataNormalizer\ruleset.json`

It is shared because DJ Library and foobar must execute identical normalization semantics. It is not a substitute for foobar-specific component configuration.

## Desired-state plan

For each physical source file, the planner eventually emits a single desired-state record containing:

- exact source path;
- exact source identity / subsong evidence;
- exact metadata input fingerprint;
- exact ruleset revision;
- selected metadata proposals;
- canonical metadata document;
- evaluated filename;
- evaluated destination directory;
- final target path;
- file action: none / rename / move / copy;
- safety classification;
- rule provenance;
- conflict state.

Virtual subsongs may contribute metadata evidence but must not create duplicate physical file operations.

## Approved safety improvements

### Plan fingerprint

Approval is bound to the complete computed plan, not merely the tag proposal.

The future apply gate must invalidate the plan when any relevant input changes, including:

- metadata fingerprint;
- active ruleset revision;
- source path;
- naming/routing rule inputs;
- computed target path.

A stale plan is never partially trusted.

### Whole-batch preflight

Before the first mutation, the component evaluates the entire selected batch.

Preflight must detect at minimum:

- multiple sources targeting the same destination;
- destination already exists;
- source disappeared or moved;
- illegal/unsupported target paths;
- empty filename/directory outcomes;
- duplicate physical operations caused by subsongs;
- unresolved REVIEW proposals;
- any stale metadata/ruleset/plan binding.

Foreseeable conflicts are reported before writes begin.

### Native title formatting for naming and routing

Filename and destination expressions should use foobar2000's own title-formatting implementation where the SDK exposes it.

The project must not grow an independent parser for `%field%`, `$if()`, or related foobar syntax.

Naming/routing evaluation always runs against the planned canonical metadata, so a metadata normalization and the resulting filename/path are one coherent transaction plan.

### Apply report

After execution, the component shows a compact host-native result report with counts and per-item failures/skips:

- metadata changed;
- renamed;
- moved;
- copied;
- unchanged;
- skipped;
- failed.

The report is traceability, not an independent backup database.

## Write ordering

The intended executor sequence is:

1. revalidate the complete plan;
2. apply approved metadata changes through supported foobar SDK APIs;
3. obtain/confirm the resulting host metadata state;
4. execute approved rename/move/copy through supported host services where available;
5. collect final outcomes;
6. show the apply report.

Exact ordering may be refined if the foobar SDK's file-operation services impose a stronger transactional contract, but it must never bypass stale-plan validation.

## Physical vs virtual metadata

The project must preserve the safety lesson proven by `foo_metadata_isolation`: a projected virtual-subsong `file_info` is not automatically a safe authority for physical tags.

Before tag writes are enabled, the foobar adapter needs a dedicated physical-vs-embedded-cuesheet qualification gate.

Generic virtual writes that could reconstruct incomplete physical metadata are forbidden.

## User-facing scope

The normal workflow should ultimately be:

**Prepare Tracks... → Analyze → Unified Preview → Apply**

The preview combines:

- metadata changes;
- final filename;
- final destination;
- action;
- SAFE / CONFIDENT / REVIEW;
- conflicts and stale-state status.

No background write or automatic file move is permitted.


## SDK planner qualification note

The public foobar2000 SDK exposes `titleformat_compiler` as a core service and a standard `file_info`-based evaluation path. The planner therefore evaluates naming/routing expressions against an in-memory canonical metadata projection using the host compiler; it does not implement foobar title-format syntax itself.

The public SDK also exposes low-level filesystem move/copy primitives and file-operation notifications, but the project has not yet identified a parameterized public service that executes foobar's built-in File Operations command with its complete user policy. Therefore the current milestone is deliberately planning-only. The eventual executor remains gated on an explicit host-policy mapping for timestamp and File Operations behavior.
