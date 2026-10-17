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

### Requirement: Shift-click queues consecutive towers for one Builder

While placing towers, Shift-left-click SHALL append sites to the selected
Builder's FIFO queue without exiting placement mode. Each accepted site
SHALL reserve its entire footprint. The Builder SHALL start the next
valid queued site only after completing the current tower. Canceling or
replacing the active order SHALL release future queued reservations.

#### Scenario: Multiple Shift-click placements
- **WHEN** a Builder Shift-clicks three distinct valid locations
- **THEN** the first tower starts immediately and the other two are built
  sequentially in the original click order

#### Scenario: Unavailable queued footprint
- **WHEN** the Builder tries to Shift-click an occupied or reserved site
- **THEN** the queue remains unchanged and the preview is invalid

#### Scenario: Construction is canceled
- **WHEN** an active build order is canceled or replaced
- **THEN** all future queued footprints belonging to that Builder are released

#### Scenario: Next site becomes unreachable
- **WHEN** a queued site cannot be built when it reaches the front
- **THEN** the Builder skips that site and tries the next queued location

### Requirement: Queued sites have persistent foundation markers

Every accepted queued tower location SHALL immediately create a world
Foundation entity associated with its owner, Builder, and reserved
footprint. The foundation SHALL remain visible and pickable until work
begins there or the order is canceled. Canceling an order SHALL remove
its unbuilt marker as well as its reservation.

#### Scenario: Queue three sites
- **WHEN** a Builder Shift-clicks three valid locations
- **THEN** each has a visible foundation marker before work begins there

#### Scenario: Builder reassigns work to a queued foundation
- **WHEN** a Builder is selected and the player right-clicks one of that
  Builder's queued foundation markers
- **THEN** every other active and pending construction order is canceled
- **AND** the other queued foundations and reservations disappear
- **AND** the selected foundation remains and construction begins there

#### Scenario: Builder targets its active foundation
- **WHEN** the player right-clicks the foundation for the current order
- **THEN** the current job continues without losing progress
- **AND** all other pending construction orders are canceled

#### Scenario: Another Builder attempts to retask a foreign foundation
- **WHEN** the selected Builder does not own the foundation's order
- **THEN** no construction order is changed

### Requirement: No Ctrl-click construction shortcut

The Ctrl-left-click development shortcut SHALL NOT place towers.
Construction placement SHALL use the Build Tower action and its
ordinary/Shift-click modes.

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

### Requirement: Selected towers show information, not construction actions

The HUD SHALL distinguish a selected Tower from a selected Builder using
the entity type reported by the simulation. Only a selected owned Builder
SHALL enable tower construction, whether activated by QML, B or Ctrl-click.
A selected tower SHALL display current HP and its configured damage and
armour when available; unavailable combat attributes SHALL be labelled
as not defined rather than guessed.

#### Scenario: Tower is selected
- **WHEN** the current player selects an owned Tower
- **THEN** the HUD shows the Tower title and HP, damage and armour fields,
  and no Build Tower action is offered

#### Scenario: Tower selected and build hotkey pressed
- **WHEN** the current player has a Tower selected and presses B or Ctrl-clicks
- **THEN** no Builder construction order is issued and placement mode stays off

#### Scenario: Builder replaces Tower selection
- **WHEN** an owned Builder is selected after a Tower
- **THEN** the Build Tower action becomes available and the Tower-only
  damage and armour fields are hidden

#### Scenario: Health changes after selection
- **WHEN** a selected Tower's health changes during construction or combat
- **THEN** the HUD shows the updated current health and maximum health
