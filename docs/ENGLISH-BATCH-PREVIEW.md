# English-only foobar UI and editable batch preview

Status: **feature-source implementation; no MSVC/foobar host qualification and no writes**

This feature branch extends the previous native Prepare Tracks dialog branch,
which in turn depends on the unmerged batch planner / external CUE work.
`main` remains untouched.

## Permanent product UI language policy

All **built-in user-facing component text** must be English, including
context-menu names and descriptions, Preferences, modal dialogs,
warnings, preview reports, status labels and error messages.

Do not translate, normalize or otherwise alter **user content** to satisfy
this policy. Artist/title tags, `DATE`/`DATE_RAW`, user-defined folder
names, user's own Masstagger/foobar presets (including the historical
profile name `Alben`) and customizable menu caption preferences are
data and retain their original values.

The legacy German hardcoded UI from earlier development branches was
converted to English. A static audit now flags the previously used
German UI phrases and requires the English strings.

## First editable native batch grid

The primary `Prepare Tracks (Preview)...` command now opens the existing
one-time profile editor (Singles / Albums / Live Sets / Custom); choosing
Preview opens a **native virtualized ListView table**, rather than a
large popup text report.

Columns:

- Source file
- Profile
- Proposed raw target
- Status

The table uses `LVS_OWNERDATA` so row widgets are not duplicated for
each selected media file. An initial host read-only analysis builds
canonical **SAFE-only** projected metadata once per selected foobar item
and compiles naming expressions using the real foobar Title Formatting
service. The source metadata fingerprints are rechecked before showing
or recalculating any preview.

The bottom editing controls accept a routing profile, display name,
destination root, and native Title Formatting expression. The user may
select individual / multiple table rows and click **Apply to selected**,
or choose **Apply to all**. These actions are **preview-only**: they
re-evaluate affected naming results in temporary in-memory copies.
One failure aborts the UI change as a whole; a successful change
recomputes all raw destination duplicates.

The portable `djmeta::describe_batch_preview()` supplies explicit
read-only statuses:

- `PHYSICAL_SOURCE_UNQUALIFIED`: possible virtual or duplicate physical file;
- `TARGET_EXPRESSION_EMPTY`;
- `DUPLICATE_RAW_TARGET`: byte-identical raw output only;
- `UNAPPROVED_METADATA_PROPOSALS`;
- `CUE_DEPENDENCIES_UNCHECKED`;
- `FILESYSTEM_TARGET_UNCHECKED`.

This first UI **never** marks a row ready to write merely because a
raw output path looks valid. For now CUE and target checks are always
unqualified; real OS canonicalization, target existence probes and
foobar File Operations sanitizing are not assumed.

Historical Singles / Alben / Livesets routing expressions stay
unchanged in the reference fixtures. Displayed built-in UI captions
and the preset picker are English, while user-editable profile names
retain their source spelling unless edited.

## Work remaining before a qualified user test

- MSVC Win32/x64 compilation and real foobar v2.x UI smoke tests;
- DPI layout at 100–200%, Dark Mode and keyboard navigation checks;
- handling large library selections without excessively long synchronous
  UI analysis;
- proper per-row display of proposed tag diffs, approval states and all
  CUE/sidecar dependencies;
- full host-canonical path inspection, File Operations filename filtering,
  live target identity guards and plan fingerprint integration;
- editable actions Copy / Move / Rename / None and batch overwrite decisions;
- persistent user-defined routing profiles with host Preferences semantics;
- a writer/executor only after every safety and host policy gate qualifies.

No tag writes, file operations, automatic overwrite, background writes,
or persistent route changes are implemented here. The two label Preferences
and in-session route editing features are intentionally separate.

## Build and CI governance

The initial portable C++ raw collision preview model was compiled and
tested with Clang C++20, `-Wall -Wextra -Wpedantic -Werror`,
AddressSanitizer and UndefinedBehaviorSanitizer without GitHub Actions.
New native dialog compilation has not yet been qualified on Windows.

All feature-branch GitHub commits use `[skip ci]`; do not create a
PR targeting main or trigger workflows while the Actions runner quota
and startup problem persists. No unqualified merge or release.
