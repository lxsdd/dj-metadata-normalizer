import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rules_path = root / "rules" / "default-rules.json"
schema_path = root / "rules" / "schema-v1.json"

ruleset = json.loads(rules_path.read_text(encoding="utf-8"))
schema = json.loads(schema_path.read_text(encoding="utf-8"))

assert schema["properties"]["schema_version"]["const"] == 1
assert ruleset["schema_version"] == 1
assert ruleset["ruleset_id"]
assert ruleset["revision"]

allowed_match = {"always", "exact"}
allowed_transform = {"trim_whitespace", "collapse_whitespace", "replace_with"}
allowed_safety = {"SAFE", "CONFIDENT", "REVIEW"}
allowed_source = {"builtin", "migrated_masstagger", "user_correction", "manual"}

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
        assert rule["transform"]["kind"] in {"trim_whitespace", "collapse_whitespace"}, (
            f"SAFE rule uses unqualified semantic transform: {rule['id']}"
        )

print(f"PASS: ruleset schema-v1 structural contract, {len(ids)} unique rules")
