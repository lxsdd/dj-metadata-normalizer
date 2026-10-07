import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rules_path = root / "rules" / "default-rules.json"
schema_v1_path = root / "rules" / "schema-v1.json"
schema_v2_path = root / "rules" / "schema-v2.json"

ruleset = json.loads(rules_path.read_text(encoding="utf-8"))
schema_v1 = json.loads(schema_v1_path.read_text(encoding="utf-8"))
schema_v2 = json.loads(schema_v2_path.read_text(encoding="utf-8"))

assert schema_v1["properties"]["schema_version"]["const"] == 1
assert schema_v2["properties"]["schema_version"]["const"] == 2
assert ruleset["schema_version"] in {1, 2}
assert ruleset["ruleset_id"]
assert ruleset["revision"]

allowed_match = {"always", "exact"}
allowed_transform_by_schema = {
    1: {"trim_whitespace", "collapse_whitespace", "replace_with"},
    2: {"normalize_unicode_whitespace", "trim_whitespace", "collapse_whitespace", "replace_with"},
}
safe_transform_by_schema = {
    1: {"trim_whitespace", "collapse_whitespace"},
    2: {"normalize_unicode_whitespace", "trim_whitespace", "collapse_whitespace"},
}
allowed_safety = {"SAFE", "CONFIDENT", "REVIEW"}
allowed_source = {"builtin", "migrated_masstagger", "user_correction", "manual"}

schema_version = ruleset["schema_version"]
allowed_transform = allowed_transform_by_schema[schema_version]
safe_transform = safe_transform_by_schema[schema_version]

ids = set()
for rule in ruleset["rules"]:
    required = {"id", "enabled", "priority", "fields", "match", "transform", "safety", "source"}
    assert required <= rule.keys(), f"missing required keys in {rule.get('id')}"
    assert rule["id"] not in ids, f"duplicate rule id: {rule['id']}"
    ids.add(rule["id"])
    assert isinstance(rule["enabled"], bool)
    assert isinstance(rule["priority"], int)
    assert rule["fields"] and all(isinstance(x, str) and x for x in rule["fields"])
    assert rule["match"]["kind"] in allowed_match
    assert rule["transform"]["kind"] in allowed_transform
    assert rule["safety"] in allowed_safety
    assert rule["source"]["kind"] in allowed_source
    assert rule["source"].get("rationale")

    if rule["match"]["kind"] == "exact":
        assert "value" in rule["match"], f"exact match requires value: {rule['id']}"
    if rule["transform"]["kind"] == "replace_with":
        assert "replacement" in rule["transform"], f"replace_with requires replacement: {rule['id']}"

    if rule["safety"] == "SAFE":
        assert rule["transform"]["kind"] in safe_transform, (
            f"SAFE rule uses unqualified semantic transform: {rule['id']}"
        )

if schema_version == 1:
    assert all(
        rule["transform"]["kind"] != "normalize_unicode_whitespace"
        for rule in ruleset["rules"]
    )

print(
    f"PASS: ruleset schema-v{schema_version} structural contract, "
    f"{len(ids)} unique rules; v1 compatibility schema retained"
)
