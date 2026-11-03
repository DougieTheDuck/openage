# Spec Delta

## ADDED Requirements

### Requirement: Builder flare

A Builder player SHALL be able to flare a map position. Every Builder SHALL
see a star in the flaring player's colour at that position for 5 seconds and
hear the flare sound once. The Hunter SHALL NOT see or hear flares, and SHALL
NOT receive them from the host.

#### Scenario: Flare seen by teammates
- **WHEN** player 3 presses F and clicks a tile
- **THEN** players 2-8 see a player-3-coloured star there for 5 seconds and hear a gling

#### Scenario: Hunter excluded
- **WHEN** a Builder flares
- **THEN** the Hunter's client receives no flare data and shows nothing

#### Scenario: Spam
- **WHEN** a Builder flares twice within 2 seconds
- **THEN** the host ignores the second flare

#### Scenario: Hunter tries to flare
- **WHEN** a client playing the Hunter sends a flare command
- **THEN** the host rejects it
