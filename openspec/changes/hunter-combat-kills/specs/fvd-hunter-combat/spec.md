# Spec Delta

## ADDED Requirements

### Requirement: Hunter horses spawn on a simulation-time cadence

While a match is running, the simulation SHALL spawn one Hunter-owned horse
at the center of the map for each elapsed second of active game time.

#### Scenario: One second elapses
- **WHEN** a full second of match simulation time elapses
- **THEN** one new horse owned by player 1 has spawned at the central spawn location

#### Scenario: Match is not running
- **WHEN** the game is in the lobby or stopped
- **THEN** no timed Hunter horses spawn

### Requirement: Hunter horses move in the playable world

Spawned horses SHALL be represented as movable world units. Their initial
automated route and available human Hunter commands SHALL be resolved in
design before declaring horse behavior complete.

#### Scenario: Horse enters play
- **WHEN** a scheduled horse is spawned
- **THEN** all clients may observe the same horse and its authoritative position

### Requirement: Completed towers fire arrows and can kill horses

Completed Builder towers SHALL autonomously fire arrows at eligible Hunter
targets, applying simulation-owned damage that can lead to a single death.

#### Scenario: Horse is in attack range
- **WHEN** a live Hunter horse is a valid target of a finished tower
- **THEN** the tower fires arrows and applies damage according to game rules

#### Scenario: Construction is unfinished
- **WHEN** a construction order has not yielded a completed tower
- **THEN** it cannot fire arrows or score kills

### Requirement: Kill attribution is unique and belongs to the tower owner

The simulation SHALL increment only the owning player's kill total when
that player's tower deals the lethal hit to a Hunter unit, with at most one
credit per killed unit.

#### Scenario: Builder's tower kills a horse
- **WHEN** a tower owned by player 4 deals lethal damage to a Hunter horse
- **THEN** player 4's tower kill count increases by one

#### Scenario: Competing lethal hits
- **WHEN** multiple tower shots could resolve around a horse's death
- **THEN** exactly one valid lethal event is credited and no unit is double-counted

#### Scenario: Hunter or unrelated damage occurs
- **WHEN** an event does not involve a Builder-owned tower dealing lethal damage
- **THEN** it does not increment a Builder's tower kill count

### Requirement: Local score remains private to the player

The UI SHALL show only the current player's own credited kill total.

#### Scenario: Another Builder scores
- **WHEN** a remote Builder destroys a horse
- **THEN** the local Builder's visible kill count does not change
