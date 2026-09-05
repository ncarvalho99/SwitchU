# Implementation Tasks: Idempotent Platform-Picker Availability Results

## Package 1: Client-side generation gating (`PlatformPickerScreen.hpp`)

**Goal**: Pin every availability task/result to its `m_generation`.

1. Add `uint64_t m_availabilityResultsGeneration` to store the active `m_generation` for each platform's `m_availabilityResults[i]`.
2. While launching `/v1/availability`, capture the current `m_generation` and store it with the `std::shared_ptr<std::atomic<Availability>>`.
3. In `onContentUpdate()`, only apply result if `result->generation == m_generation` AND `m_active`.
4. In `hide()`, do not clear `m_generation` but also do not publish it further; pending items are recognized as stale.

**Tests**: Build menu, open picker three times for unchanged title; verify output doesn't change.

**Requirement Addressed**: FR-001, FR-002.

## Package 2: Backend transient-failure protection (`metacritic_proxy.py`)

**Goal**: Never save transient failures as negative cache entries.

5. Verify `_igdb_availability()` error paths for 429/timeout/connection error.
6. Ensure `_save_cached()` hits only when IGDB returns a finalized "found"/"not found" conclusion.
7. If transient errors occur, return stale cached result or 502/504 without writing to `score_cache`.

**Tests**: Run proxy `py_compile`, inject simulated 429, confirm no negative entry written.

**Requirement Addressed**: FR-003, FR-004.

## Package 3: Validation & Commit

8. Run full repro (menu build, picker open/reopen times) to confirm SC-001, SC-002, SC-003.
9. Update changelog: `docs/chapters/changelogs/CHANGELOG_65#[...]` with succinct description.
10. Commit with message per AGENTS.md: `fix(menu): make platform-picker availability idempotent across reopen`.

**Tests**: Build `sysmodule`, deploy proxy, reproduce.

**Users**: End user who opened picker repeatedly across sessions.