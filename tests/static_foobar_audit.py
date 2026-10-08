from pathlib import Path

root = Path(__file__).resolve().parents[1]
foobar = root / "src" / "foobar"
sources = "\\n".join(
    p.read_text(encoding="utf-8", errors="strict")
    for p in sorted(foobar.rglob("*"))
    if p.suffix.lower() in {".cpp", ".h", ".rc"}
)

required = {
    "preview menu": "Normalize metadata (Preview)",
    "async metadata load": "load_info_async",
    "read-only shared rules": "GENERIC_READ",
    "existing-file rules open": "OPEN_EXISTING",
    "shared rules path": "DJMetadataNormalizer",
    "explicit no-write UX": "No tags were written",
}
missing = [name for name, token in required.items() if token not in sources]
if missing:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: missing " + ", ".join(missing))

forbidden = [
    "update_info_async",
    "update_info_simple",
    "metadb_io_v2::update",
    "file_info_filter",
    "GENERIC_WRITE",
    "WriteFile(",
    "DeleteFile",
    "MoveFile",
    "ReplaceFile",
    "fileOpenWrite",
    "filesystem::g_open_write",
]
found = [token for token in forbidden if token in sources]
if found:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: write-capable token(s): " + ", ".join(found))

project = (root / "foo_dj_metadata_normalizer.vcxproj").read_text(encoding="utf-8")
for token in [
    "FOOBAR2000_TARGET_VERSION=81",
    "src\\core\\normalizer.cpp",
    "src\\core\\rule_loader.cpp",
    "foo_dj_metadata_normalizer",
]:
    if token not in project:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: project contract missing " + token)

preview = (foobar / "preview.cpp").read_text(encoding="utf-8")
for token in [
    "item.result.input_fingerprint",
    "djmeta::fingerprint(current)",
    "The preview is stale",
]:
    if token not in preview:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: stale-input preview guard missing " + token)

planner = (foobar / "titleformat_planner.cpp").read_text(encoding="utf-8")
for token in [
    "titleformat_compiler::get()",
    "compiled->run_simple",
    "__meta_add_unsafe_ex",
    "meta_add_value_ex",
]:
    if token not in planner:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: host-native planner contract missing " + token)
for forbidden in [
    "$if(",
    "%artist%",
    "std::filesystem",
    "MoveFile",
    "CopyFile",
    "filesystem::g_move",
    "filesystem::g_copy",
]:
    if forbidden in planner:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: planner bypasses host-native read-only boundary: " + forbidden)

route_menu = (foobar / "context_menu.cpp").read_text(encoding="utf-8")
menu_settings = (foobar / "menu_settings.cpp").read_text(encoding="utf-8")
menu_preferences = (foobar / "menu_preferences.cpp").read_text(encoding="utf-8")
prepare_dialog = (foobar / "prepare_dialog.cpp").read_text(encoding="utf-8")
batch_table_dialog = (foobar / "batch_preview_dialog.cpp").read_text(encoding="utf-8")
layout_storage = (foobar / "batch_table_settings.cpp").read_text(encoding="utf-8")
route_preview = (foobar / "routing_preview.cpp").read_text(encoding="utf-8")
legacy_profiles = (foobar / "legacy_routing_profiles.h").read_text(encoding="utf-8")
resources = (foobar / "component.rc").read_text(encoding="utf-8")

for label, source, token in [
    ("default Singles caption", menu_settings, "Prepare: Singles (Preview)"),
    ("default Alben caption", menu_settings, "Prepare: Albums (Preview)"),
    ("default Livesets caption", menu_settings, "Prepare: Live Sets (Preview)"),
    ("default main caption", menu_settings, "Prepare Tracks (Preview)"),
    ("cfg-backed captions", menu_settings, "cfg_string"),
    ("stable individual caption GUID", menu_settings, "guid_caption_primary"),
    ("dynamic captions", route_menu, "effective_menu_caption(index)"),
    ("new main route command", route_menu, "guids::prepare_tracks"),
    ("main dialog dispatch", route_menu, "show_prepare_tracks_dialog(retained)"),
    ("menu count", route_menu, "get_num_items() override { return 5; }"),
    ("foobar menu DEFAULT_ON", route_menu, "contextmenu_item::DEFAULT_ON"),
    ("foobar menu DEFAULT_OFF", route_menu, "contextmenu_item::DEFAULT_OFF"),
    ("host cfg Preferences", menu_preferences, "preferences_page_v3"),
    ("host Apply", menu_preferences, "void apply() override"),
    ("host Reset", menu_preferences, "void reset() override"),
    ("host staged changes", menu_preferences, "preferences_state::changed"),
    ("host dark-mode Preferences", menu_preferences, "preferences_state::dark_mode_supported"),
    ("native prefs dialog", resources, "IDD_MENU_PREFERENCES DIALOGEX"),
    ("unified single window", prepare_dialog, "show_batch_preview_dialog(handles, choice)"),
    ("real metadata proposals", batch_table_dialog, "djmeta::describe_metadata_diffs"),
    ("one row per physical selected track", batch_table_dialog, "djmeta::summarize_track_changes"),
    ("track list as production widget", batch_table_dialog, "IDC_METADATA_TRACK_LIST"),
    ("track master selection", batch_table_dialog, "LVN_ITEMCHANGED"),
    ("selection resolves source identity", batch_table_dialog, "state->track_view_order[row]"),
    ("track-specific detail", batch_table_dialog, "djmeta::selected_track_diffs"),
    ("music versus extended focus", batch_table_dialog, "MetadataFocus::Music"),
    ("extended/custom fields reachable", batch_table_dialog, "MetadataFocus::All"),
    ("track list sorted independently", batch_table_dialog, "sort_track_summaries"),
    ("actual tag diff provenance tooltip", batch_table_dialog, "LVN_GETINFOTIPW"),
    ("metadata routing fields hidden", batch_table_dialog, "IDC_BATCH_ROUTE_LABEL"),

    ("metadata and file tab control", batch_table_dialog, "TCN_SELCHANGE"),
    ("metadata diff view", batch_table_dialog, "IDC_METADATA_LIST"),
    ("metadata safety", batch_table_dialog, "item.safety"),
    ("metadata rule provenance in production tooltip", batch_table_dialog, "entry.rule_ids"),
    ("metadata sort", batch_table_dialog, "sort_metadata_diff_rows"),
    ("source path presentation only", batch_table_dialog, "display_file_path"),
    ("unified tabs resource", resources, "IDC_BATCH_TABS"),
    ("virtualized batch table resource", resources, "IDD_BATCH_PREVIEW DIALOGEX"),
    ("virtualized ListView", batch_table_dialog, "ListView_SetItemCountEx"),
    ("header drag/drop", batch_table_dialog, "LVS_EX_HEADERDRAGDROP"),
    ("sort on header click", batch_table_dialog, "LVN_COLUMNCLICK"),
    ("virtual view identity mapping", batch_table_dialog, "state->view_order[view_index]"),
    ("selected row identity", batch_table_dialog, "state.view_order[static_cast<std::size_t>(index)]"),
    ("persistent column order", batch_table_dialog, "ListView_GetColumnOrderArray"),
    ("persistent column widths", batch_table_dialog, "ListView_GetColumnWidth"),
    ("column visibility", batch_table_dialog, "visible_mask"),
    ("column header popup", batch_table_dialog, "show_column_menu"),
    ("reliable header submenu", batch_table_dialog, "SetWindowSubclass"),
    ("header submenu teardown", batch_table_dialog, "RemoveWindowSubclass"),
    ("resizable table layout", batch_table_dialog, "resize_batch_dialog"),
    ("resize minimum", batch_table_dialog, "WM_GETMINMAXINFO"),
    ("reset column layout", batch_table_dialog, "Reset column layout"),
    ("layout settings read", batch_table_dialog, "load_batch_table_layout()"),
    ("layout settings persist", batch_table_dialog, "store_batch_table_layout(state->layout)"),
    ("foobar profile layout cfg", layout_storage, "cfg_string"),
    ("batch table model", batch_table_dialog, "djmeta::describe_batch_preview"),
    ("group route override", batch_table_dialog, "apply_to_rows(dialog, *state, true)"),
    ("selected route override", batch_table_dialog, "apply_to_rows(dialog, *state, false)"),
    ("selection stale guard", batch_table_dialog, "verify_snapshot(entry)"),
    ("per-batch atomic UI edits", batch_table_dialog, "state.entries = std::move(candidate)"),
    ("unverified CUE gate", batch_table_dialog, "entry.input.cue_dependencies_checked = false"),
    ("unverified destination gate", batch_table_dialog, "entry.input.filesystem_target_checked = false"),
    ("safe-only projection", route_preview, "djmeta::stage_safe_only"),
    ("foobar titleformat projection", route_preview, "evaluate_titleformat_against_canonical"),
    ("stale metadata guard", route_preview, "djmeta::fingerprint(latest)"),
    ("subsong physical guard", route_preview, "get_subsong_index() != 0"),
    ("raw target duplicate warning", route_preview, "raw_target_counts"),
    ("no-write banner", route_preview, "No tags were written"),
]:
    if token not in source:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: missing " + label)

# Product policy: every built-in user-visible label must be English.
# Original media tags / externally supplied routing profile values may
# legitimately use other languages and must never be transliterated.
non_english_ui_literals = [
    "Metadaten normalisieren", "Tracks vorbereiten",
    "Vorbereiten:", "Die Vorschau", "Es wurden keine",
    "Zielordner:", "Dateinamensmuster:",
]
for ui_file in ("preview.cpp", "routing_preview.cpp", "prepare_dialog.cpp",
                "context_menu.cpp", "menu_preferences.cpp", "menu_settings.cpp",
                "component.rc"):
    text = (foobar / ui_file).read_text(encoding="utf-8")
    for forbidden_ui in non_english_ui_literals:
        if forbidden_ui in text:
            raise SystemExit("STATIC FOOBAR AUDIT FAIL: non-English UI in " +
                             ui_file + ": " + forbidden_ui)

if "IDC_METADATA_TRACK_LIST" not in resources or "IDC_METADATA_FILTER" not in resources:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: track master/metadata filter missing from production resources")
if "ShowWindow(state.metadata_track_list" not in batch_table_dialog:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: master/detail tab visibility not wired")
if "IDD_PREPARE_TRACKS DIALOGEX" in resources:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: obsolete two-dialog wizard remains")
if "contextmenu_item::FORCE_OFF" in route_menu:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: host visibility disabled")
for token in [
    r"Z:\Music\Singles",
    r"Z:\Music\Alben",
    r"Z:\Music\Livesets",
    "%album artist%/%album%/%artist% - %title%",
    "%album artist%/%album%[ '('%date%')']/%tracknumber%. %artist% - %title%",
]:
    if token not in legacy_profiles:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: legacy route fixture drift: " + token)
for unsafe in [
    "update_info_async", "GENERIC_WRITE", "MoveFile(", "CopyFile(",
    "DeleteFile(", "filesystem::g_move", "filesystem::g_copy",
]:
    if unsafe in route_preview or unsafe in prepare_dialog or unsafe in menu_preferences or unsafe in batch_table_dialog:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: preview includes write path: " + unsafe)
for path in [
    r"src\foobar\routing_preview.cpp",
    r"src\foobar\batch_preview_dialog.cpp",
    r"src\core\batch_preview.cpp",
    r"src\core\table_layout.cpp",
    r"src\core\track_review.cpp",
    r"src\foobar\batch_table_settings.cpp",
    r"src\foobar\prepare_dialog.cpp",
    r"src\foobar\menu_settings.cpp",
    r"src\foobar\menu_preferences.cpp",
    r"src\core\staging.cpp",
    r"src\core\routing_overrides.cpp",
]:
    if path not in project:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: source missing from Win32/x64: " + path)

rules_runtime = (foobar / "rules_runtime.cpp").read_text(encoding="utf-8")
if "CreateFileW" not in rules_runtime or "GENERIC_READ" not in rules_runtime:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: shared rules are not opened read-only")
if "CREATE_ALWAYS" in rules_runtime or "OPEN_ALWAYS" in rules_runtime:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: rules runtime contains a file-creation path")

# The GUI contract is backed by an actual Windows HWND runtime test.
# Static checks here only verify that production uses the tested adapter.
native_controls = (foobar / "native_preview_controls.h").read_text(encoding="utf-8")
gui_test = (root / "tests" / "native_preview_controls_tests.cpp").read_text(encoding="utf-8")
if 'IDC_BATCH_PROFILE_NAME' in resources or 'IDC_BATCH_NAME_LABEL' in resources:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: duplicate route profile input still instantiated")
if 'CBS_DROPDOWN | CBS_AUTOHSCROLL' not in resources:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: routing combo is not editable")
if 'read_control(window, IDC_BATCH_PROFILE_PICKER)' not in batch_table_dialog:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: custom route picker not used for profile name")
if 'align_native_preview_form(dialog)' not in batch_table_dialog:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: common layout adapter not wired")
if 'GetComboBoxInfo' not in native_controls or 'CreateDialogParamW' not in gui_test:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: native runtime resource/alignment test missing")

if 'IDC_METADATA_TRACK_FILTER' not in resources or 'IDC_METADATA_TRACK_FILTER_LABEL' not in resources:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: track discovery widgets missing")
if 'id == IDC_METADATA_TRACK_FILTER && HIWORD(wp) == CBN_SELCHANGE' not in batch_table_dialog:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: track discovery control event not handled")
if 'djmeta::filter_track_view' not in batch_table_dialog:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: filtered master data not wired")
if '{IDC_METADATA_TRACK_FILTER_LABEL, IDC_METADATA_TRACK_FILTER}' not in native_controls:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: native status filter alignment not wired")
print("PASS: foobar preview is analysis-only; no tag/file write path is present")
