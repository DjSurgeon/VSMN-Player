---
name: migrate
description: >-
  Run or inspect the project migrations across backends. Use when the user says "run migrations", "migration status", or "/migrate".
---


Action: the user's request

Drive the migration lifecycle through the project's task runner (detect it with `.agents/tools/facts.sh`,
run it under `.agents/tools/watch.sh`). Do NOT hand-edit migrations here — to
AUTHOR a new migration use `the migrate-db skill`.

## Workflow

### Phase 1 — Inspect

- Run the project's migrate-status command — applied vs pending.
- Locate the migration files in the project (note any gaps in the numbering).

### Phase 2 — Apply (confirm first — DB writes are irreversible)

- `status`  → the project's migrate-status command
- `all`     → the project's migrate-all command
- `backend` → the project's per-backend migrate command   (needs that backend's services up)
- no arg    → the project's default migrate command

### Phase 3 — Verify

- Re-run the migrate-status command; confirm idempotency (re-applying is a no-op).
