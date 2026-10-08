# Native foobar context-menu customization and routing overrides

Status: **design + core implemented; full Preferences editor and foobar GUI unqualified**.

## What foobar itself can do

Use `File > Preferences > Display > Context Menu` to manage command
presence and hierarchy. The plugin must let foobar manage its own command
visibility; it must not create a competing checkbox for that same setting.

The SDK's `contextmenu_item::get_enabled_state()` returns a **stable default**:
`DEFAULT_ON` for the main **Metadaten normalisieren (Vorschau)**
command and `DEFAULT_OFF` for the three specialist
Singles / Alben / Livesets previews. The user can enable the latter in
foobar's existing menu editor or access optional commands through host
Shift-menu behavior where supported.

Existing command GUIDs **must remain stable** so context-menu and
keyboard-shortcut mappings are not lost when the wording changes.

foobar's menu editor is not a promise of arbitrary renaming or freely
sorting all commands. Menu item name customization and user-defined
routing profiles therefore require a *small host-native plugin Preferences
editor*, not an unsupported assumption about foobar's own UI.

## Planned own Preferences scope (user-requested)

- User-editable caption for the single primary `Tracks vorbereiten...`
  command; optionally captions for explicit route actions.
- User-maintained routing profiles with titles and native foobar
  Title Formatting expressions (the user is not restricted to three
  historical presets).
- Optional groups where technically available; avoid conflicting
  with existing foobar menu visibility and shortcut settings.
- Configuration stored via stable GUIDs and foobar's `cfg_*` service.
- Preferences `Apply / Cancel / Reset` with staging; no instant persistence
  while typing; dark mode and DPI via host conventions.
- Menu-command identity remains stable even if display text changes.

This editor **has not yet been created**, and user-visible arbitrary
menu captions are therefore not offered by the current build.

## Actual routing override implementation

`djmeta::apply_routing_override()` is a portable, read-only function
that accepts an explicit route choice with one of two scopes:

- a non-empty set of unique selected physical audio IDs, or
- all physical audio entries in the current batch.

It accepts arbitrary user-provided route identifier and naming expression,
rather than hardcoding the three reference profiles. It also accepts the
desired file action (none, rename, move or copy).

It refuses ambiguous/duplicate physical IDs, unknown selections, empty
naming choices for file actions, and control characters.

The result is a **new immutable in-memory batch proposal**:

- raw metadata identity and selected canonical metadata fingerprints are
  retained;
- source identity is preserved;
- the affected audio's profile, naming expression, action and manual flag
  are changed;
- host-computed target paths, identity keys, target-presence guard and
  previously verified cue links are cleared;
- **all external CUE plans in the current batch** are invalidated
  conservatively after any route change: the current item model has only
  one primary associated audio ID, while multi-FILE cues can refer to
  several selected or unselected sources;
- other companion plans are invalidated when their primary audio is
  selected; full host dependency mapping remains a prerequisite before
  enabling any file writes;
- the original plan is never mutated.

A new foobar Title Formatting evaluation, source/target existence scan,
companion/CUE mapping and whole-batch preflight are mandatory after
changing the route. The old fingerprint no longer authorizes Apply.

## Scope boundary

This code is a **planner-only foundation**. It does not yet provide:

- a visually editable per-item foobar grid;
- host-native titleformat rerendering after a user edit;
- physical path normalization or Windows target collision probing;
- on-disk CUE changes, metadata updates or other file writes.

Never call `review_batch_plan().ready_to_apply` proof that a foobar
host-side operation is safe without full SDK path and cuesheet qualification.
