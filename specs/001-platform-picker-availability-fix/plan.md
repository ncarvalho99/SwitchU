# Implementation Plan: Idempotent Platform-Picker Availability Results

**Branch**: `001-platform-picker-availability-fix` | **Date**: 2026-09-05 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-platform-picker-availability-fix/spec.md`

## Summary

Fix the non-idempotent platform-picker availability behavior reported by the
user across repeated close/reopen cycles for unchanged titles. The technical
approach is dual-layered and conservative:

1. **Client-side generation gating in the menu**:
   Bind every in-flight availability task and every stored result to the exact
   `m_generation` active when `showForTitle()` launched it. When
   `onContentUpdate()` polls completed futures, drop any result whose
   generation does not match `m_generation`. On `hide()`, also mark the active
   generation obsolete so that pending responses cannot update the screen state
   post-close or bleed into an immediate reopen.
2. **Backend transient-failure protection in the proxy**:
   In `web/metacritic-proxy/metacritic_proxy.py`, ensure `/v1/availability`
   never writes a transient failure (HTTP 429, upstream timeout, connection
   reset, Twitch token outage) into `score_cache` as a `found: false` entry.
   Only genuine, completed IGDB queries that returned an empty match set may be
   cached as negative results.

## Technical Context

**Language/Version**: C++20 (menu sysmodule), Python 3.10+ (proxy service)

**Primary Dependencies**: `lib/nxui` (threading, UI), `themeshop::http` (CURL wrapper), FastAPI, SQLite3

**Storage**: SQLite3 (`score_cache` table in proxy)

**Testing**: Python proxy verification (`py_compile`, unit/regression assertions for caching behavior), Docker cross-build (`tools/build-local.ps1`), hardware deploy to `G:`

**Target Platform**: Nintendo Switch (Horizon OS sysmodule) + Linux server (Oracle Cloud metadata proxy)

**Project Type**: System UI sysmodule + companion backend proxy

**Performance Goals**: Sub-second cached response, zero visible stutter on reopen

**Constraints**: Respect proxy 20-req/60s rate limit, no breakage of `/v1/metadata`, no full-dossier calls inside picker

**Scale/Scope**: 1 screen (`PlatformPickerScreen.cpp`), 1 proxy file (`metacritic_proxy.py`)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Principle I (Repository Protocol Is Supreme)**: PASS — edits guarded by active lock, zero out-of-scope files touched.
- **Principle II (Smallest Verified Change)**: PASS — targeted fix in `PlatformPickerScreen.cpp` and `metacritic_proxy.py` without modifying visuals, i18n, or unrelated screens.
- **Principle III (Documented, Traceable Evidence)**: PASS — each phase tracked in state/specs, final change will generate a changelog entry.
- **Principle IV (Cache and Endpoint Isolation)**: PASS — `avail:v1:` cache logic remains completely isolated from `igdb:v4:` full dossier cache.
- **Principle V (Production Boundaries)**: PASS — local verification and explicit approval required before deployment.

## Project Structure

### Documentation (this feature)

```text
specs/001-platform-picker-availability-fix/
├── spec.md              # Feature specification
├── plan.md              # This implementation plan
├── checklists/
│   └── requirements.md  # Quality checklist
└── tasks.md             # Work packages and execution tasks
```

### Source Code Changes

```text
projects/menu/src/settings/
├── PlatformPickerScreen.hpp # Add generation tracking to results/tasks
└── PlatformPickerScreen.cpp # Filter results by generation, obsolete on hide

web/metacritic-proxy/
└── metacritic_proxy.py      # Guard _save_cached on availability against transient errors
```

## Proposed Architecture and Component Changes

### Component 1: `PlatformPickerScreen` (C++)

- **Responsibility**: Render platform cards and display current availability.
- **Change**:
  - Update `m_availabilityResults` to store both `Availability` and `uint64_t generation`.
  - In `onContentUpdate()`, only update `m_platforms[i].availability` if `result->generation == m_generation` AND `m_active` is true.
  - In `hide()`, ensure pending items are recognized as retired/stale.
- **Requirement Addressed**: FR-001, FR-002, SC-001, SC-002.

### Component 2: `metacritic_proxy.py` (Python)

- **Responsibility**: `/v1/availability` endpoint and caching.
- **Change**:
  - Audit `_igdb_availability()` error propagation.
  - Verify that `_save_cached` is only called when IGDB query completes successfully with a definitive match or non-match.
  - Ensure any 429/timeout raises 502/504 or returns stale cache without saving a negative entry.
- **Requirement Addressed**: FR-003, FR-004, SC-003.

## Validation Strategy

1. **Proxy syntax & regression test**:
   - `python -m py_compile web/metacritic-proxy/metacritic_proxy.py`
   - Test cache isolation and error handling with local unit check.
2. **Menu compilation**:
   - Run Docker cross-build: `tools/build-local.ps1 -Mode release -Variant sysmodule`.
3. **Traceability**:
   - Ensure all acceptance criteria in `spec.md` are covered by tasks.
