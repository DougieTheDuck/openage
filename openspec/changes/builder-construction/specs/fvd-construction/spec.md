# Spec Delta

## Purpose

FVD construction turns a Builder player's tower-placement request into a
validated, interruptible gameplay order that requires movement and elapsed
simulation time before a tower becomes usable.

## ADDED Requirements

### Requirement: Only an eligible Builder may receive a build order

The simulation SHALL accept a tower build order only from a Builder player
controlling a living Builder they own and whose civilization can build towers.

#### Scenario: Owned Builder starts an order
- **WHEN** player 2 orders their selected Builder to build a tower at an eligible location
- **THEN** that Builder begins a tower construction order attributed to player 2

#### Scenario: Another player's unit or Hunter tries to build
- **WHEN** a player submits an order through player 1, an unowned Builder, or a non-Builder entity
- **THEN** the simulation rejects it and no tower, reservation or construction is created

### Requirement: Build tiles are valid and reserved consistently

The simulation SHALL accept only buildable, unoccupied map tiles and SHALL
prevent two outstanding construction orders from claiming the same tile.

#### Scenario: A requested tile is already occupied or reserved
- **WHEN** a second order targets a tile holding a unit, tower or outstanding building reservation
- **THEN** the second order is rejected without affecting the first order

#### Scenario: Simultaneous orders claim one tile
- **WHEN** two valid requests for the same tile are processed
- **THEN** the first accepted order owns that tile and the other is rejected

#### Scenario: Out-of-bounds request
- **WHEN** a Builder attempts to build beyond the map boundary
- **THEN** the request is rejected without mutating game state

### Requirement: Builder reaches an adjacent stand location

An accepted order SHALL move its Builder to a reachable, unoccupied location
adjacent to the target footprint, rather than moving onto the tower tile.

#### Scenario: Distant construction
- **WHEN** an owned Builder is distant from a valid tower tile
- **THEN** the Builder walks toward a reachable adjacent stand location and construction does not complete during travel

#### Scenario: No reachable stand location
- **WHEN** no eligible adjacent location can be reached
- **THEN** the order fails without creating a tower and its tile reservation is released

#### Scenario: Builder already adjacent
- **WHEN** the Builder is already at an eligible adjacent location
- **THEN** it can begin construction without a redundant travel order

### Requirement: Construction requires positive simulation time

The tower SHALL become a completed game entity only after the assigned
Builder reaches the site and the configured construction duration elapses.

#### Scenario: Completion after arrival
- **WHEN** the Builder reaches a valid site and stays on the order through the construction duration
- **THEN** one completed tower is created on the reserved tile for the Builder's player

#### Scenario: Tower is not available immediately
- **WHEN** a construction order is pending or underway but has not completed
- **THEN** it does not yield a completed, combat-capable tower

### Requirement: Interrupted orders release their reservation

The simulation SHALL cancel incomplete work when the Builder receives a
replacement order, dies, or cannot continue construction.

#### Scenario: Player redirects the Builder
- **WHEN** the Builder receives a new move order before tower completion
- **THEN** the former build order is canceled and no tower appears later from it

#### Scenario: Builder disappears before completion
- **WHEN** the Builder is destroyed during travel or construction
- **THEN** the build order is canceled and its reservation is released

### Requirement: Build orders use authoritative simulation state

The simulation SHALL decide validity, completion and ownership using game
state at the time the order or completion event is processed.

#### Scenario: Placement is contested after travel
- **WHEN** the site or Builder becomes invalid before completion
- **THEN** completion fails safely with no duplicated tower or stale reservation

#### Scenario: Renderer is not running
- **WHEN** a valid construction order runs in a headless simulation
- **THEN** movement and completion produce the same rules outcome as with the graphical client
