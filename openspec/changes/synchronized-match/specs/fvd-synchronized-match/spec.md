# Spec Delta

## ADDED Requirements

### Requirement: Host determines the shared match state

The host SHALL maintain authoritative simulation state, regardless of
whether the host participant was assigned Hunter or Builder.

#### Scenario: Guest constructs a tower
- **WHEN** a connected guest submits a valid owned Builder construction intent
- **THEN** the host processes it and every client eventually sees the same accepted order and tower

#### Scenario: Host is assigned Hunter
- **WHEN** the Hunter lottery assigns player 1 to the host
- **THEN** the host still runs the authoritative simulation without acquiring Builder privileges

### Requirement: Client actions are constrained to assigned ownership

The host SHALL bind every remote session to its assigned player ID and
SHALL reject commands to control other players' entities or abilities.

#### Scenario: Forged Builder order
- **WHEN** a client assigned player 3 sends a command referencing player 2's Builder
- **THEN** the host rejects it and leaves game state unchanged

#### Scenario: Human Hunter tries to construct
- **WHEN** the participant assigned player 1 sends BuildTower
- **THEN** the host rejects the construction request

### Requirement: Observable game state is synchronized

All clients SHALL receive authoritative updates for unit positions,
reserved/building/completed tower state, combat, deaths and kills.

#### Scenario: Two Builders order the same site
- **WHEN** two connected Builders simultaneously target the same free tile
- **THEN** the host accepts at most one reservation and all clients converge on that outcome

#### Scenario: Hunter horse spawns
- **WHEN** a second of match simulation time elapses
- **THEN** every connected client observes the same spawned Hunter horse

#### Scenario: Tower scores a kill
- **WHEN** the host confirms a tower's lethal attack
- **THEN** all clients remove the same killed horse and the owning player's local kill total increases once

### Requirement: Invalid inputs and disconnects are visible

The client SHALL handle rejected commands and lost connections without
silently presenting an independently diverging simulation.

#### Scenario: Invalid command
- **WHEN** the host rejects a client action
- **THEN** the client receives or can derive a rejection and does not treat it as a confirmed world mutation

#### Scenario: Guest disconnects
- **WHEN** a participant loses connectivity during a match
- **THEN** remaining clients are informed and do not silently create conflicting ownership or state

### Requirement: Windows direct-IP multiplayer works

The initial networking implementation SHALL allow connected Windows
clients on reachable addresses/ports to play a common match.

#### Scenario: Two Windows machines
- **WHEN** two compatible Windows clients are connected and the host starts
- **THEN** both clients can observe shared movements, builds, horse attacks and kill outcomes
