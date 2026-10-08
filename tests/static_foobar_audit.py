from pathlib import Path

root = Path(__file__).resolve().parents[1]
foobar = root / "src" / "foobar"
sources = "\\n".join(
    p.read_text(encoding="utf-8", errors="strict")
    for p in sorted(foobar.rglob("*"))
    if p.suffix.lower() in {".cpp", ".h", ".rc"}
)

required = {
    "preview menu": "Metadaten normalisieren (Vorschau)",
    "async metadata load": "load_info_async",
    "read-only shared rules": "GENERIC_READ",
    "existing-file rules open": "OPEN_EXISTING",
    "shared rules path": "DJMetadataNormalizer",
    "explicit no-write UX": "Es wurden keine Tags geschrieben",
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
    "Die Vorschau wurde als veraltet verworfen",
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
route_preview = (foobar / "routing_preview.cpp").read_text(encoding="utf-8")
legacy_profiles = (foobar / "legacy_routing_profiles.h").read_text(encoding="utf-8")
for label, token in {
    "single route menu": "Vorbereiten: Singles (Vorschau)",
    "album route menu": "Vorbereiten: Alben (Vorschau)",
    "live sets route menu": "Vorbereiten: Livesets (Vorschau)",
    "safe-only projection": "djmeta::stage_safe_only",
    "foobar titleformat projection": "evaluate_titleformat_against_canonical",
    "stale metadata guard": "djmeta::fingerprint(latest)",
    "subsong physical guard": "get_subsong_index() != 0",
    "raw target duplicate warning": "raw_target_counts",
    "no-write banner": "Es wurden keine Tags geschrieben",
}.items():
    source = route_menu if "menu" in label else route_preview
    if token not in source:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: route preview missing " + label)
for token in [
    r"Z:\Music\${k}",
    r"Z:\Music\${k}",
    r"Z:\Music\${k}",
    "%album artist%/%album%/%artist% - %title%",
    "%album artist%/%album%[ '('%date%')']/%tracknumber%. %artist% - %title%",
]:
    if token not in legacy_profiles:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: legacy route fixture drift: " + token)
for unsafe in [
    "update_info_async", "GENERIC_WRITE", "MoveFile(", "CopyFile(",
    "DeleteFile(", "filesystem::g_move", "filesystem::g_copy",
]:
    if unsafe in route_preview:
        raise SystemExit("STATIC FOOBAR AUDIT FAIL: route preview includes write path: " + unsafe)
if 'src\\foobar\\routing_preview.cpp' not in project:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: route preview omitted from Win32/x64 project")
if 'src\\core\\staging.cpp' not in project:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: staging core omitted from Win32/x64 project")

rules_runtime = (foobar / "rules_runtime.cpp").read_text(encoding="utf-8")
if "CreateFileW" not in rules_runtime or "GENERIC_READ" not in rules_runtime:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: shared rules are not opened read-only")
if "CREATE_ALWAYS" in rules_runtime or "OPEN_ALWAYS" in rules_runtime:
    raise SystemExit("STATIC FOOBAR AUDIT FAIL: rules runtime contains a file-creation path")

print("PASS: foobar preview is analysis-only; no tag/file write path is present")
