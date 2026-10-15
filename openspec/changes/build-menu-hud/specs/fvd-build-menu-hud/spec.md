# Spec Delta

## ADDED Requirements

### Requirement: Owned Builders can place towers through the UI

The UI SHALL offer a Build Tower action when the local player has an
eligible owned Builder selected, with placement preview and cancellation.

#### Scenario: Builder starts placement
- **WHEN** the current Builder player selects their owned Builder and activates Build Tower
- **THEN** the UI enters placement mode and indicates the prospective tower tile

#### Scenario: User chooses a valid tile
- **WHEN** the user confirms an available tile
- **THEN** a validated simulation build order is issued for their Builder

#### Scenario: User chooses an invalid tile
- **WHEN** the preview is over an unavailable site
- **THEN** the UI indicates invalid placement and no tower order is issued

#### Scenario: User cancels placement
- **WHEN** the user presses Escape or the cancel mouse action
- **THEN** placement mode exits without placing a tower or moving the Builder

### Requirement: Hunter has no build action

Player 1 SHALL have no enabled construction menu, even though that player
is a human participant in a multiplayer match.

#### Scenario: Hunter selection
- **WHEN** the local player is assigned the Hunter role
- **THEN** Build Tower is absent or disabled

### Requirement: Local HUD shows only local kill count

The HUD SHALL display the current player's own authoritative tower kill
count and SHALL NOT expose other players' kill totals.

#### Scenario: Match begins
- **WHEN** a local player's match UI appears
- **THEN** the displayed kill total is zero

#### Scenario: A player's tower scores a kill
- **WHEN** the simulation credits a lethal hit to the local player's tower
- **THEN** that player's displayed kill count increases once

#### Scenario: Other player's tower scores
- **WHEN** the simulation credits another player's tower
- **THEN** the local HUD kill count remains unchanged and the other total is not shown

### Requirement: HUD interactions do not leak into world input

UI button presses SHALL not issue unintended world selection or movement
commands.

#### Scenario: Build button clicked
- **WHEN** the user clicks a Build Tower HUD control
- **THEN** the button action occurs without a world click or order
