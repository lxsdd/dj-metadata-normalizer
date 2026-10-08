# Opt-in semantic REVIEW candidates

These six intentionally synthetic mapping examples demonstrate the existing
schema-v2 `exact` + `replace_with` support for ARTIST, LABEL, TITLE, VERSION,
GENRE and collaboration text.

- **All candidates are disabled.** They are not part of
  `rules/default-rules.json`, are never loaded by the foobar default runtime,
  and must not be silently merged into a user's live rule configuration.
- Every semantic replacement stays **REVIEW**. Accepting a proposal in the
  in-memory preview is not permission to write tags or move files.
- TITLE/version splitting is not implemented by this example. A whole TITLE
  replacement does **not** populate VERSION. Likewise, replacing a literal
  artist string with `Artist A; Artist B` is not an ID3 multivalue operation.
- Replace placeholder mappings only with confirmed collection-specific
  references and add acceptance/holdout tests. Never infer genre/label/artist
  aliases or semantic splits from whitespace rules.
- Common rule ownership remains here; downstream DJ Library consumes the
  versioned native engine, and the Bridge remains read-only.

This is a test fixture and rule-authoring example, **not a production ruleset**.
