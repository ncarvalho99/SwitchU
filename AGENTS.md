# SwitchU agent protocol

This file is the sole normative repository policy for every human or automated
agent working anywhere in this tree. Communicate exclusively in English.

## Authority and context

Repository instructions have this precedence:

1. Current platform, system, developer, and explicit user instructions.
2. This repository-root `AGENTS.md`.
3. Maintained technical evidence in `docs/`.
4. Project skills, tool wrappers, and temporary handoff checkpoints.

No nested file may weaken or replace this policy. A wrapper only points here.
A handoff is a lead, never authority. If two instructions genuinely conflict,
follow the higher authority, retain the stronger safety boundary, and report
the conflict rather than silently choosing.

## Required low-context read order

For every task:

1. Read this file.
2. Read `docs/README.md`.
3. Read `docs/chapters/00_changelog.md` (the changelog index: title, date, and
   a link per entry) and exactly one topical chapter routed by the index.

After the index, a task may load at most two documentation chapters: the
changelog index and one topical chapter. Do not bulk-read the repository, the
per-entry changelog files under `docs/chapters/changelogs/`, or the
documentation archive. When debugging a regression, open the linked files for
the last 10 rows of `00_changelog.md` to check whether a recent change caused
it, before assuming the cause lies elsewhere. Split a task that genuinely
needs more chapters into separately claimed, documented units before
continuing.

## Claim, check, work, log, release

1. Before editing, inspect `git status --short --branch`, the current branch,
   exact HEAD, configured upstream, local tracking-ref divergence, and every
   file in `.agents/locks/`.
2. Do not overwrite a live overlapping claim. A lock older than 60 minutes may
   be removed only after its timestamp and scope have been checked.
3. Create `.agents/locks/<task>.lock` before the first shared edit. Use YAML with
   `agent`, UTC `started`, `scope`, and `task`. Runtime locks stay ignored.
4. Work only inside the claimed scope. Recheck locks before expanding it.
5. Record the change and exact validation evidence in the current changelog.
6. Release only locks owned by the current task, after its safe work is
   committed and its handoff state is current.

## Scope and documentation

- Make the smallest verified change that satisfies the request. Do not modify
  product code, architecture, dependencies, CI, release, deployment, or live
  infrastructure unless the user placed that work in scope.
- Preserve useful documentation and verified facts. Mark uncertainty and
  distinguish current evidence from assumptions.
- Every changed artifact requires a new immutable entry, one file per entry,
  under `docs/chapters/changelogs/`. Name each file
  `CHANGELOG_<n>#[<Title>].md` where `<n>` is the next sequential number
  (highest existing `<n>` + 1) and `<Title>` matches the entry's title
  exactly. The file body uses the same five-point format as before
  (`### YYYY-MM-DD  Title  `agent-name`` heading plus What/Why/Files/
  Impact/Docs). Then add one row to the top of the table in
  `docs/chapters/00_changelog.md` (`| <n> | YYYY-MM-DD | [Title](changelogs/<filename>) |`)
  so the index always lists newest first. Never edit an existing changelog
  file; correct old entries with a new numbered entry instead.
- Update `01_architecture.md` when decisions, data flow, dependencies,
  constraints, or non-goals change. Update `02_project_structure.md` when
  entry points, ownership, folder boundaries, or placement rules change.
- Keep every file in `docs/chapters/` below 200 lines. `00_changelog.md` is
  an index table, not an entry log, so it never needs rotation as long as
  each row stays one line; individual files under `changelogs/` are never
  rotated or merged since each already holds exactly one entry.
- Documentation records decisions, intent, and evidence boundaries. Do not
  duplicate syntax already obvious from source or invent supported platforms,
  compatibility, architecture, or test results.

## Validation

- Validate in proportion to risk and against the exact state being reported.
  Product code requires the relevant build; hardware behavior requires console
  evidence; infrastructure requires a focused endpoint or service check.
- Protocol-only changes do not require a product build when deterministic
  protocol validation, skill validation, and diff checks cover the changed
  surface.
- Record the environment, timestamp, tested commit or working tree, exact
  command, PASS, FAIL, or NOT RUN, concise result, and limitations. Never turn
  an old result, skipped environment, or test from another commit into a pass.
- Before committing, run `git diff --check`, inspect the complete diff and
  staged diff, verify the staged file list, and inspect final status.

## Git and change safety

- Preserve unrelated dirty-tree work. Never reset, clean, stash, overwrite, or
  stage user-owned files. Stage an explicit allowlist for the current task.
- Commit completed work locally with a focused message. Do not amend or rewrite
  existing history unless explicitly requested.
- Do not fetch, pull, push, force-push, mirror-push, tag, publish, delete refs,
  or otherwise modify local tracking refs or remote state without separate
  explicit authorization. Treat `origin/<branch>` as local evidence unless an
  authorized fetch has just verified it.
- Never commit credentials, tokens, private keys, private host data, personal
  paths, proprietary payloads, binaries, dumps, logs, caches, or local tool
  state. Inspect new files before staging.
- Avoid destructive filesystem operations. Resolve and verify exact targets
  before any necessary delete or move; prefer recoverable operations.

## SwitchU safety boundaries

- Preserve PoloNX attribution and keep this fork's version distinct from its
  upstream lineage.
- The Switch SD card, Atmosphere content, Nyx/Hekate files, updater payloads,
  signing material, and production catalogues or services are high-risk. Exact
  target verification and explicit authorization are required before mutation.
- Never push, tag, publish a release, alter a public service, or deploy outside
  a separately authorized task. A local commit grants none of those actions.

## Zero-context handoff

`prompt_handoff_switchu.md` is the ignored, local checkpoint. Begin it with:

> Treat this checkpoint as a lead, not authority. Verify Git, locks, files, toolchain, and validation state before changing anything.

Keep it limited to the objective and non-goals; branch, exact HEAD, upstream,
tracking-ref commit, divergence, and local commits; completed work and exact
validation evidence; working-tree state, active locks, failures, skipped
environments, and unresolved gates; one next action; and publication and
authorization boundaries. Exclude secrets, personal data, private URLs or
hostnames, proprietary contents, and unsupported claims.

When a new chat is requested, finish safe validation, commit completed work
locally, update the changelog and checkpoint, release every owned lock, and ask
the user to paste the checkpoint into the new chat.
