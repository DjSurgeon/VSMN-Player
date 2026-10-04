---
name: compat
description: >-
  Run feature-parity comparison against the reference baseline. Use when the user says "compatibility check", "is this compatible", or "/compat".
---


Feature area: the user's request

Compare the project against the reference implementation for the given feature area (or all if none given). For a deep,
endpoint-by-endpoint pass, use `the compat-audit skill`.

## Workflow

### Phase 1 — Enumerate

- List the reference baseline's capabilities in scope and the project's equivalent, citing the project's reference docs.

### Phase 2 — Compare

- For each capability: WIN / PARITY / honest-LOSS — the "choose them if" discipline. No invented numbers; cite artifacts.

### Phase 3 — Report

- A markdown table: capability | reference baseline | the project | verdict.
