# Spec Delta

## Purpose

Builder-issued construction orders require valid ownership, movement to an
adjacent stand tile and a finite period before a usable tower exists.

## ADDED Requirements

### Requirement: Only an eligible Builder may construct

The simulation SHALL accept construction orders only for a living Builder
owned by the requesting player, who SHALL be in a Builder slot (2-8).
Player 1, the human Hunter role, SHALL never build structures.

#### Scenario: Human Builder submits an eligible order
- **WHEN** a player assigned slot 2-8 orders their own selected Builder to build on a free tile
- **THEN** the simulation accepts that Builder's construction order

#### Scenario: Hunter or other player's Builder submits an order
- **WHEN** player 1 submits any tower order, or another player references a Builder they do not own
- **THEN** the simulation rejects it without constructing or reserving a tower

#### Scenario: Solo Hunter role is selected
- **WHEN** the solo tester switches to Hunter player 1 and attempts to build
- **THEN** the same Hunter construction prohibition applies

### Requirement: Build sites are validated and exclusively reserved

The simulation SHALL allow construction only on unoccupied, buildable,
in-bounds tiles and SHALL reserve the accepted target until completion or
cancellation.

#### Scenario: Same tile is requested twice
- **WHEN** two players issue valid Builder orders for the same free tile
- **THEN** the first accepted order reserves it and the next is rejected

#### Scenario: Occupied, blocked or out-of-bounds tile
- **WHEN** a request targets a tile occupied by an entity, reserved for construction, unbuildable or outside the map
- **THEN** the order is rejected without changing an existing order

### Requirement: Builder reaches a stand tile next to the build footprint

The simulation SHALL route an accepted Builder to a reachable vacant
location adjacent to the target tile rather than onto the target itself.

#### Scenario: Distant Builder
- **WHEN** a Builder requests a valid distant tile
- **THEN** the Builder travels toward an eligible neighboring position and does not finish during travel

#### Scenario: Already adjacent
- **WHEN** the Builder already stands at an eligible neighboring location
- **THEN** construction may begin without further travel

#### Scenario: No valid path
- **WHEN** no eligible adjacent location can be reached
- **THEN** the order fails and releases its reservation

### Requirement: Construction completes after simulation time

The simulation SHALL finish one tower only after the Builder reaches
the valid site and a positive configured construction duration elapses.

#### Scenario: Construction duration elapses
- **WHEN** the Builder stays on the valid order through the full construction duration after arrival
- **THEN** exactly one finished tower appears at the reserved tile with that Builder's owner

#### Scenario: Construction is pending
- **WHEN** a construction order has not completed
- **THEN** the site SHALL NOT contain a usable combat-capable tower

### Requirement: Interruption cancels unfinished work

The simulation SHALL cancel construction and release reservations when
the Builder is redirected, destroyed or cannot continue the order.

#### Scenario: Player redirects Builder
- **WHEN** a new Move or Build order supersedes an incomplete order
- **THEN** the previous order is canceled, its reservation released, and its delayed event cannot spawn a tower

#### Scenario: Builder dies before completion
- **WHEN** a Builder is removed before finishing construction
- **THEN** no tower is completed by that order and its reservation is released

### Requirement: Simulation rules do not depend on the UI

Construction validation, travel and completion SHALL use simulation state
and SHALL produce the same outcome in headless and graphical execution.

#### Scenario: Construction runs headlessly
- **WHEN** a valid order is processed without a renderer
- **THEN** the same movement, ownership and completion rules apply

#### Scenario: Site is invalidated before completion
- **WHEN** a previously valid site or order ceases to be valid
- **THEN** completion does not create a duplicated tower or stale reservation
