# Configurable virtual batch preview table

Status: SOURCE IMPLEMENTED / portable C++ checks PASS / MSVC and foobar host test PENDING.

This feature extends feature/english-batch-preview-20261008, itself on top
of the open, unmerged normalizer planner/CUE development branches. No
tag/file writes are added. The host GUI has not been built or exercised on
Windows and is NOT a qualified installable candidate.

## Interaction contract

The first four built-in columns are Source file, Profile,
Proposed raw target and Status. The native SysListView32 stays
in virtual (LVS_OWNERDATA) mode.

### Sorting
- Click any logical column header to sort ascending.
- Click that header again to switch to descending.
- Sorting is stable; equal keys retain original input sequence.
- Sorting is a display-only mapping. The original selected track, canonical
  metadata, CUE dependencies and associated file identity do not change.
- Selection and multi-selection translate between displayed positions and
  stable underlying row indices, even after sorting.
- A route edit recalculates the preview, duplicates and displayed sorting.
- Current portable collation folds ASCII case. Full locale-aware/natural
  Unicode sorting is not yet qualified and must not be implied.

### Columns
- Drag headers to reorder with native LVS_EX_HEADERDRAGDROP.
- Drag column boundaries to resize. Widths are stored in 96-DPI logical
  units and scaled to current system DPI on restore.
- Right-click a header to hide/show individual columns using a checked menu.
- Never hide the final visible column.
- Reset column layout restores the original order, widths, visibility
  and default sorting.

### Persistence

On closing the preview dialog, the component stores logical column order,
widths, visible columns and sorting in foobar's native cfg_string
(configuration GUID). Hidden columns retain their previous width. No
external JSON file, Windows registry key or bridge state is involved.
Per-instance/profile settings reload with the next dialog.

The format is versioned and validated. Corrupted, unsupported or impossible
layouts (duplicate column IDs, no visible columns, invalid widths, bitmask
or sort index) fall back to defaults.

Layout persistence is independent of routing choices and never authorizes
a file operation or overwrite.

## Qualification

The portable C++20 layout model was exercised locally using Clang with
AddressSanitizer and UndefinedBehaviorSanitizer, and GCC; both enabled
-Wall -Wextra -Wpedantic -Werror. The tests cover strict roundtrip,
invalid-setting fallback, ascending/descending stable sorting and a
20,000+ row view permutation. No GitHub Actions runner minutes were used.

Static source checks cover native reordering, resizing, visibility, sorting,
identity mapping, profile persistence and no-write boundaries.
Static inspection is not a Windows build.

## Explicitly incomplete

- Visual/MSVC foobar2000 2.x Win32/x64 test, actual header right-click,
  Dark Mode, DPI, keyboard navigation and drag/drop acceptance.
- Arbitrary additional metadata columns are not yet exposed. The four
  implemented columns are customizable. More columns require the future
  metadata-field/sidecar registry.
- Per-row editable metadata diffs, real destination canonicalization,
  CUE/sidecar verification, overwrites and actual executor.
- Persistent filters and named saved view layouts are separate features
  not implied by this column-layout implementation.

The built-in plugin UI remains English; user metadata, profile names
and naming expressions preserve the original language.
