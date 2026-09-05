# Feature Specification: Idempotent Platform-Picker Availability Results

**Feature Branch**: `001-platform-picker-availability-fix`

**Created**: 2026-09-05

**Status**: Draft

**Input**: User report: opening "Mark as game port" three times for
"Simpsons Hit and Run" first showed three available platforms, then two,
and then reported no metadata available.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reopen a title's platform picker reliably (Priority: P1)

A user selecting "Mark as game port" for an unchanged title can close and
immediately reopen the platform picker without the available-platform set
changing merely because the screen was reopened.

**Why this priority**: This is the reported regression and the core value
of the picker. Incorrectly hiding a known available platform makes the
metadata-selection flow unreliable.

**Independent Test**: Open the picker for the same title three times in a
row, close it between opens, and compare the displayed platform statuses.

**Acceptance Scenarios**:

1. **Given** a title with one or more confirmed available original platforms,
   **When** the user opens, closes, and reopens the picker three times without
   changing the title or server data, **Then** each open displays the same
   status for every platform.
2. **Given** a title with no confirmed platform metadata, **When** the user
   reopens the picker, **Then** each open consistently reports no matching
   metadata rather than fluctuating result sets.

---

### User Story 2 - Distinguish temporary lookup failure (Priority: P2)

A user encountering a temporary availability lookup failure is not told
that metadata is definitively unavailable, and a temporary failure does
not poison later lookups for that title/platform pair.

**Why this priority**: A false permanent negative result defeats the
30-day cache's purpose and makes a transient server or network issue look
like a content fact.

**Independent Test**: Simulate an availability lookup rejection or timeout,
then repeat the lookup after the failure condition clears and confirm a
previously available platform remains selectable/available.

**Acceptance Scenarios**:

1. **Given** an availability lookup cannot complete temporarily, **When** the
   user views the picker, **Then** the affected platform is not presented as a
   confirmed "metadata unavailable" result solely because of that failure.
2. **Given** a temporary lookup failure has cleared, **When** the user opens
   the picker again, **Then** the platform receives a normal fresh or cached
   result rather than a cached false negative from the failed lookup.

---

### Edge Cases

- A user closes the picker while some platform checks are still pending and
  reopens it before those earlier checks finish.
- A platform check completes after a newer picker open has already started.
- The service rate limit, an upstream timeout, or an upstream server error
  occurs while multiple platform checks are pending.
- A genuine "not found" result must remain distinguishable from a temporary
  lookup failure and may still be cached according to the established
  availability-cache policy.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST present a stable availability status for every
  platform when the same title's picker is opened repeatedly without an
  intervening title or server-data change.
- **FR-002**: The system MUST ensure a result started for an earlier picker
  open cannot alter the status displayed by a later picker open.
- **FR-003**: The system MUST distinguish a completed "metadata unavailable"
  result from a temporary inability to determine availability.
- **FR-004**: The system MUST NOT persist a temporary availability lookup
  failure as a cached "metadata unavailable" result.
- **FR-005**: The system MUST preserve the existing fast availability lookup
  behavior and the separation between availability data and full metadata
  dossier data.
- **FR-006**: The system MUST preserve the existing platform picker visual
  design and localized strings unless a minimal status-state change is
  required to meet FR-003.

### Key Entities *(include if feature involves data)*

- **Picker open**: One user-visible session of the platform picker for a
  title; it owns a set of in-flight availability lookups and displayed
  platform statuses.
- **Platform availability result**: A status associated with one
  title/platform pair: available, unavailable, or temporarily undetermined.
- **Availability cache entry**: A cached result for a title/platform pair,
  whose lifetime and eligibility depend on whether the result was a completed
  content determination rather than a temporary failure.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a three-consecutive-open reproduction for the same unchanged
  title, the displayed status set is identical across all three opens.
- **SC-002**: In a close/reopen-before-completion reproduction, zero results
  from the earlier open are applied to the later open's displayed status set.
- **SC-003**: Under a simulated temporary availability failure, zero failed
  lookups are stored as a confirmed unavailable cache entry.
- **SC-004**: A normal completed availability lookup remains fast and does not
  invoke the full metadata dossier lookup solely to determine platform
  availability.

## Assumptions

- The reported three-open sequence occurred with no deliberate change to the
  title, its selected platform, or the service configuration between opens.
- The exact root cause is not yet confirmed; implementation may be client-side,
  server-side, or both, but must remain within the stated feature scope.
- Existing backend cache behavior for a genuine successful "not found" result
  remains valid unless investigation establishes otherwise.
