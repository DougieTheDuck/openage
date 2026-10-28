# Spec Delta

## ADDED Requirements

### Requirement: One permanent central Hunter tower

At match initialization, the simulation SHALL create exactly one Hunter-owned spawn tower at map center, recognizable as a distinct permanent structure. It SHALL reserve its complete footprint and SHALL remain in the world for the entire match regardless of attacks, damage, Builder tower-demolition commands, ordinary tower-destruction cleanup or reconnection.

#### Scenario: Match starts
- **WHEN** a new FVD match initializes
- **THEN** exactly one special Hunter tower exists at the map center, owned by player 1
- **AND** its footprint blocks ordinary Builder tower placement and traversal

#### Scenario: Players attempt to destroy it
- **WHEN** a player attempts to damage, delete, demolish or otherwise destroy the central Hunter tower
- **THEN** the command or damage is rejected or ignored without removing or reducing the tower
- **AND** all connected clients continue to see it

### Requirement: Spawn horses around the permanent tower

The simulation SHALL spawn each scheduled Hunter horse in a valid location surrounding the central tower, outside its blocked footprint, without issuing automatic movement or attack orders. The first spawn SHALL remain at t=20 seconds and additional horses SHALL spawn once per second thereafter. Spawn-site selection SHALL be deterministic and cope with occupied preferred sites.

#### Scenario: First and following waves
- **WHEN** simulation time reaches 20, 21 and 22 seconds
- **THEN** three Hunter horses have spawned in valid positions around the central tower, one at each second
- **AND** none occupies the tower footprint or receives an automatic travel order

#### Scenario: Nearby positions become crowded
- **WHEN** the closest valid spawn positions are occupied or blocked
- **THEN** the simulation selects other valid surrounding positions deterministically without omitting the scheduled horse

### Requirement: Spawn tower activates after the Builder head start

The central Hunter tower SHALL NOT target or fire before match simulation time t=20 seconds. Starting at t=20 it SHALL automatically fire missiles at eligible living enemy Builder units within a **10-tile range**, with each confirmed impact causing **100 damage**. Its attacks SHALL be simulated by the authoritative host and replicated to guests. No Hunter-owned horse or friendly entity SHALL be targeted.

#### Scenario: Builder is in range during the head start
- **WHEN** a Builder stands within 10 tiles at t=19.999 seconds
- **THEN** the Hunter tower does not launch a missile or apply damage

#### Scenario: The timer expires
- **WHEN** simulation time reaches t=20 seconds with an eligible Builder within 10 tiles
- **THEN** the tower becomes able to acquire and attack that Builder, launching missiles according to its defined attack cadence
- **AND** each successfully resolved missile hit applies exactly 100 damage once

#### Scenario: Builder is beyond range
- **WHEN** every enemy Builder is farther than 10 tiles from the tower
- **THEN** the tower does not acquire or launch at those Builders

#### Scenario: Authoritative multiplayer combat
- **WHEN** a spawn-tower missile is launched and impacts a Builder in a multiplayer match
- **THEN** all clients receive the same authoritative projectile, HP/death and resulting world state
- **AND** the impact cannot be applied twice or forged by a client
