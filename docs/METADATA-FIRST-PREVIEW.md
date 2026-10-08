# Integrated Metadata First Preview

Status: read-only candidate pending Win32/x64 Actions and foobar hardware QA.

The one default-visible **Prepare Tracks (Preview)** command opens ONE
native batch window, instead of the former two-dialog profile wizard.
The window initially shows the **Metadata changes** tab and provides
**File locations** as the second tab.

The metadata table is sourced from real `Engine::analyze` proposals,
not guessed from filenames. Columns display source file, field, original
value, proposed value, SAFE / CONFIDENT / REVIEW classification, and
all contributing rule IDs. Multi-rule chains retain aggregate safety,
and multi-value fields retain the original per-value locations in the
portable data model. Both tab views support column header sorting and
header drag/reordering; the original file preview retains saved column
layout. The metadata table is initially review-only; it is not yet a
proposal approval editor. SAFE proposals are simulated in the naming
preview only; no tag write occurs.

The routing controls stay in the same window, and are disabled while
the metadata tab is selected to avoid implying that a tag-review action
renames a file. On File locations, use **Preview selected** or
**Preview all** to re-evaluate edited routing expressions without
editing media, and with the existing fingerprint staleness check.

User-reported usability corrections:
- A second, redundant dialog is eliminated.
- Existing Singles/Alben/Livesets profile selected in the initial
  routing choice remains selected instead of showing Custom.
- The display of foobar `file://` paths is converted to familiar
  file paths without changing host identity keys.
- The display of proposed relative targets uses Windows-style separators,
  but is **NOT** OS-canonicalization, filename sanitizing or a verified
  destination; it is explicitly labeled unverified.
- The repeated CUE/filesystem status noise is replaced by a global,
  explicit notice in the footer.
- Buttons say Preview instead of Apply when no writes are possible.
- Preferences help strings are shortened to fit narrow layouts.

Strict limitations:
1. No tag changes are accepted or committed yet. A SAFE classification
   is a rule-safety decision, not user approval.
2. The default v2 rule set currently covers Unicode/ASCII whitespace
   normalization only; historical artist/version/genre rules must be
   migrated explicitly and tested, not silently invented.
3. File routes are still an unqualified Title Formatting projection.
   CUE/sidecars, actual destination collisions and overwrite consent
   are future gates, not hidden passes.
4. User metadata, user preset profile names and DATE/DATE_RAW are never
   rewritten by UI-language cleanup. The component's own UI is English.

This change extends the analysis-only native foobar component. Full
Windows CI and later host smoke tests are mandatory before release.
