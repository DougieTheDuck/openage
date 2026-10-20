## ADDED Requirements

### Requirement: Gameplay has one authoritative observable outcome

For every gameplay-relevant entity, state transition, combat outcome,
construction or order, the Hunter SHALL publish enough authoritative data
for every connected client to present the same result. The Hunter SHALL
be the only simulation that decides ownership, targeting, movement paths,
collisions, damage, death, score and win/loss. A client MUST NOT silently
substitute independent local game simulation for missing replication.

#### Scenario: Builder constructs a tower
- **WHEN** any Builder reserves a site, walks to it, constructs and completes a tower
- **THEN** the Hunter and all Builders observe consistent reservation,
  foundation, movement, construction progress, final tower and health
- **AND** a client joining via recovery snapshot renders the correct phase.

#### Scenario: Hunter attacks a tower or unit
- **WHEN** Hunter horses move, focus targets, strike, kill or change orders
- **THEN** all clients observe the same horse identities, orders,
  trajectories, hit points, damage, entity removals and kill outcome.

### Requirement: Prefer deterministic time-indexed presentation curves

A simulation-authored future curve or timed event SHALL be transmitted and
evaluated locally wherever it can represent the result without client-side
gameplay decisions. State changes SHALL supersede old curves at an
authoritative time and baseline revision. Curves MUST have bounded,
validated, monotonically ordered keyframes and deterministic interpolation.
Clock synchronization and recovery SHALL not replay stale motion.

#### Scenario: Unit follows a precomputed route
- **WHEN** the Hunter simulation schedules position keyframes
- **THEN** clients receive the future segments with the authoritative
  position and interpolate those segments directly
- **AND** an updated route replaces the previous one without requiring
  continuous per-render-frame position messages.

#### Scenario: Timed projectile
- **WHEN** a tower fires an arrow
- **THEN** clients receive an authoritative launch identifier, start/end
  position and launch/impact times, render the ballistic arc locally,
  and expire it without a per-frame stream or removal packet
- **AND** damage only occurs via later authoritative entity state.
- **AND** short-lived shots are not silently dropped merely because
  their lifetime fits between two recipient snapshots: the host SHALL
  retain uniquely identified effect events until each recipient has had
  an opportunity to receive them or a bounded recovery policy applies.

#### Scenario: Snapshot recovery
- **WHEN** a client cannot apply a revision-contiguous delta
- **THEN** a fresh snapshot resets its trajectories, identities and
  pending presentation state, without duplicating shots or entities.

### Requirement: Visual and HUD parity

Every multiplayer gameplay feature MUST explicitly specify the state and
visual effects replicated to all relevant peers. This includes movement,
facing/animation phase, combat/impact feedback, death, construction,
visible tower ownership, attack orders, spawn/countdown state, and victory
conditions as each is introduced. Local-only UI interaction (camera,
selection, menu state) does not require network replication.

#### Scenario: Recipient privacy
- **WHEN** a Builder receives a world update
- **THEN** the world effects are identical to the Hunter and other peers
- **AND** only that Builder receives its private kill count and per-tower score
- **AND** no client receives another Builder's private score by accident.

### Requirement: Cross-platform visual integration acceptance

Regression tests SHALL exercise message validation, delta/recovery,
curve playback and authoritative command effects. A feature is complete
only when multiplayer acceptance covers Hunter and remote Builder displays
on real processes; a protocol-only test does not qualify as full gameplay
visual verification.

#### Scenario: Four and eight clients
- **WHEN** 4 or 8 clients simultaneously construct and attack
- **THEN** all surviving clients converge on entities, positions,
  health, orders and effect timelines despite packet batching and jitter.
