# Spec Delta

## ADDED Requirements

### Requirement: Hunter horses spawn on a simulation-time cadence

While a match is running, the simulation SHALL give the Builders a
20-second grace period before any Hunter horse spawns. It SHALL spawn
one Hunter-owned horse at the map center at match simulation time t=20s,
then one additional horse at each whole second t=21s, t=22s, and so on.

#### Scenario: Before the grace period ends
- **WHEN** the match has run for any time earlier than 20 seconds
- **THEN** no scheduled Hunter horses have spawned

#### Scenario: Grace period ends
- **WHEN** match simulation time reaches 20 seconds
- **THEN** exactly one Hunter-owned horse spawns at map center

#### Scenario: Subsequent seconds elapse
- **WHEN** match simulation time reaches 21 seconds, then 22 seconds
- **THEN** a second, then a third, Hunter-owned horse has spawned

#### Scenario: Match is not running
- **WHEN** the game is in the lobby or stopped
- **THEN** no timed Hunter horses spawn and lobby waiting does not consume the grace period

### Requirement: Newly spawned Hunter horses await human orders

The simulation SHALL initially spawn each Hunter-owned horse at the center;
horses SHALL NOT automatically follow a path, rally or attack without
an instruction from their human Hunter. Passive collision separation
MAY gently displace horses within the spawn area without creating Move orders.

#### Scenario: A horse spawns
- **WHEN** a Hunter horse appears at t=20 seconds or later
- **THEN** it is visible to all players and awaits an order, apart from passive collision displacement

#### Scenario: Horses accumulate
- **WHEN** the Hunter does not issue any commands across several spawn intervals
- **THEN** surviving uncommanded horses remain near the center and spread outward to make room for further spawns

### Requirement: New horses push the spawn crowd apart

The simulation SHALL create every scheduled horse even when its
spawn location is occupied by units or blocked by a completed tower.
It SHALL apply a deterministic two-dimensional contact-separation
response to overlapping idle horses, keeping them on valid terrain
and using short interpolated movement without assigning combat orders.

#### Scenario: Multiple horses spawn at the same point
- **WHEN** the Hunter leaves several consecutive horses uncommanded
- **THEN** all horses spawn on schedule and older idle horses spread outward
  rather than remaining stacked at precisely the same coordinates

#### Scenario: A tower blocks the center
- **WHEN** a completed tower occupies the center at the scheduled horse spawn time
- **THEN** the horse still appears, and contact resolution moves it toward
  an accessible position without moving through the tower's blocked footprint

#### Scenario: A horse has a manual Move order
- **WHEN** an additional horse spawns while another horse is following a Move order
- **THEN** the active horse's destination and command remain intact

### Requirement: Human Hunter can command individual horses or a group

Player 1's human Hunter SHALL be able to select one or more owned horses
and issue Move or AttackMove orders with a map destination. Only the
selected living horses owned by that Hunter SHALL receive the orders.

#### Scenario: Hunter attack-moves a group
- **WHEN** the Hunter selects several horses and issues AttackMove to a target location
- **THEN** each selected horse begins progressing toward the location under an AttackMove order

#### Scenario: Hunter orders ordinary movement
- **WHEN** the Hunter commands Move to a destination
- **THEN** selected horses travel there without actively seeking enemies en route

#### Scenario: Builder attempts to command Hunter units
- **WHEN** a Builder attempts to move or attack-move horses owned by player 1
- **THEN** the simulation rejects the order without changing their behavior

### Requirement: AttackMove engages opponents on the route

An attack-moving horse SHALL attack eligible enemy Builder-owned entities
encountered en route, then resume travel toward its original destination
after the engagement. Reaching the destination SHALL leave it awaiting
another Hunter order.

#### Scenario: Horse encounters an opponent
- **WHEN** a horse under AttackMove encounters an eligible opposing unit or structure
- **THEN** it interrupts travel and engages that target through simulation combat rules

#### Scenario: Horse defeats or loses a target
- **WHEN** an AttackMove target dies or is no longer eligible
- **THEN** a surviving horse resumes travel toward its original destination

#### Scenario: Horse reaches its destination
- **WHEN** an attack-moving horse finishes moving to its destination
- **THEN** it waits there for the next human-issued order

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
