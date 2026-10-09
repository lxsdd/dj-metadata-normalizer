# Candidate comparison — local read-only clipboard intake

**Status:** native source integrated, Win32/x64 installed-host acceptance not yet complete.
The **Candidate comparison** tab in `Music Metadata Studio → Prepare Tracks (Preview)` accepts a deliberately structured plain-text candidate that you copied or authored yourself. It **never automatically fetches** Beatport, Discogs, Deezer, Spotify, or other services, never logs into third-party sites, and never writes tags/files. It is an initial zero-cost/offline fallback alongside planned authorized provider connectors.

## How to try it (after the exact candidate build passes CI and installed-host QA)

1. Select **one real physical audio track** in foobar2000 and open **Music Metadata Studio → Prepare Tracks (Preview)**. Do not choose an embedded CUE subsong for this initial comparison.
2. Copy the following **synthetic sample** as plain text to the Windows clipboard. Substitute factual metadata for a user-chosen candidate only when known:

```text
@provider=discogs
@id=release:123456
@scope=edition
ARTIST=Example Artist
TITLE=Example Track (Extended Mix)
GENRE=House
GENRE=Deep House
DATE_RAW@edition=1998-06-15
LABEL=Example Records
```

3. Open **Candidate comparison**, click **Import clipboard** and inspect the two existing native tables: selected foobar track and `Field | Original | Proposed | Source | Status`.
4. `No change` means exact ordered multivalue equality, `Review` means new or different value, `Blocked` means structural tag, malformed value, conflicting date semantics or ambiguous local duplicate field. These are **not** approval decisions. Changing tab returns to the original normalization editor without modifying its pending choices.
5. Close the entire window with Close. **No tags, CUE records, ReplayGain, naming expressions, paths, music files or timestamp values are touched.**

## Strict input rules

- A single candidate per import: mandatory `@provider` and `@id` headers precede all fields; optional `@scope=recording|release|edition` (default `recording`).
- `@id` is a user-supplied provider object identifier (such as `release:123456`); it is not automatically verified or fetched.
- Each tag is exactly `FIELD=VALUE`; duplicate names represent ordered multivalues; the semicolon character is literal and is **never split**. Keep artist and remix information separate where possible.
- For `DATE` or `DATE_RAW`, explicitly append `@original`, `@edition`, or `@digital`: these dates have different meanings. The plugin will not guess.
- Up to 64 KiB input, 160 lines, 96 distinct fields, 32 values/field; valid UTF-8; reject unknown directives, incomplete identity, empty values and NUL bytes.
- Real foobar rule metadata remains read-only and protected; a physical track is rechecked for stale metadata/rules immediately before comparison. Virtual subsongs are not accepted as physical input during this initial host-qualified flow.
- UI does not merge manual fields into normalizer decisions or filesystem operations. User-supplied `@provider` is **not evidence of an authenticated provider API call**.
- Source field text is displayed without automatic changes to capitalization, dates, aliases, remix names or tag order.

## Known limitations and host qualification

- Plain text must be prepared in this simple syntax; copying an arbitrary Beatport/Discogs web page is **not** yet a valid structured candidate.
- This tab displays unverified source evidence and is not yet connected to a real provider search or release-level matching/approval UI; original normalizer's approvals remain independent.
- Candidate preview currently only shows individual field diffs for the selected track; no multi-source synthesis, offline saved sessions, cover import, automatic release matching, or clipboard-to-file changes.
- Mandatory next gate: test in actual foobar2000 2.x (Win32/x64), including button visibility, tab/keyboard navigation, large/multivalue Unicode, physical-vs-virtual selection, Dark Mode, DPI and clipboard unavailable/malformed behavior. GitHub Actions cannot substitute for this installed-host test.
- Run existing no-write integrity test on disposable MP3/CUE samples for installed-host acceptance. The tool intentionally has no new writer path.

## Architecture

`Windows CF_UNICODETEXT` → bounded UTF-8 conversion → `parse_manual_candidate` → `review_online_fields` (immutable field provenance/multivalue/no-op/structural guards) → same native foobar review table (display only). No file I/O or HTTP access and no control with tag-writing authority.
