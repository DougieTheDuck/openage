# Spec Delta

## ADDED Requirements

### Requirement: Hunter owns the shared match simulation

The participant assigned Hunter (player 1) SHALL own the authoritative
simulation before the first gameplay world snapshot, regardless of which
participant created the initial lobby. If the creator is Builder, the Hunter
SHALL establish an independent room and all Builders SHALL reconnect with
their original player IDs before world bootstrap. The original lobby creator
does not retain special authority once assigned Builder.

#### Scenario: Guest constructs a tower
- **WHEN** a connected guest submits a valid owned Builder construction intent
- **THEN** the host processes it and every client eventually sees the same accepted order and tower

#### Scenario: Lobby creator is assigned Hunter
- **WHEN** the Hunter lottery assigns player 1 to the original lobby creator
- **THEN** the creator runs the authoritative simulation without acquiring Builder privileges

#### Scenario: Lobby creator is assigned Builder
- **WHEN** the Hunter lottery assigns player 1 to a guest
- **THEN** every Builder, including the original creator, joins the Hunter's new independent authority room with unchanged player roles before world bootstrap
- **AND** no second role lottery or divergent simulation starts

### Requirement: Client actions are constrained to assigned ownership

The host SHALL bind every remote session to its assigned player ID and
SHALL reject commands to control other players' entities or abilities.

#### Scenario: Forged Builder order
- **WHEN** a client assigned player 3 sends a command referencing player 2's Builder
- **THEN** the host rejects it and leaves game state unchanged

#### Scenario: Human Hunter tries to construct
- **WHEN** the participant assigned player 1 sends BuildTower
- **THEN** the host rejects the construction request

### Requirement: Human Hunter controls horses through authenticated commands

The participant assigned Hunter player 1 SHALL be able to send selection-
based Move and AttackMove intents for owned horses. The host SHALL process
accepted commands and replicate their effects. Spawned horses SHALL remain
idle until such an order is accepted.

#### Scenario: Hunter selects several horses and orders AttackMove
- **WHEN** the Hunter player sends a valid AttackMove for owned horse IDs and a destination
- **THEN** the host accepts the command and all clients observe the resulting travel and engagements

#### Scenario: Uncommanded horses accumulate
- **WHEN** horses spawn and the human Hunter issues no commands
- **THEN** all connected clients observe those horses remaining at the map center

#### Scenario: Builder forges a Hunter order
- **WHEN** a Builder sends a Move or AttackMove naming player 1's horses
- **THEN** the host rejects the command without altering those horses

### Requirement: Observable game state is synchronized

All clients SHALL receive authoritative updates for unit positions,
reserved/building/completed tower state, combat, deaths and kills.

#### Scenario: Two Builders order the same site
- **WHEN** two connected Builders simultaneously target the same free tile
- **THEN** the host accepts at most one reservation and all clients converge on that outcome

#### Scenario: Hunter grace period and spawning
- **WHEN** the match runs from t=0 to just before t=20s
- **THEN** no clients observe a scheduled Hunter horse

#### Scenario: First and subsequent horses spawn
- **WHEN** host simulation time reaches t=20s and then t=21s
- **THEN** all clients observe the same first and second idle Hunter horses at the center

#### Scenario: Tower scores a kill
- **WHEN** the host confirms a tower's lethal attack
- **THEN** all clients remove the same killed horse and the owning player's local kill total increases once

### Requirement: Invalid inputs and disconnects are visible

The client SHALL handle rejected commands and lost connections without
silently presenting an independently diverging simulation.

#### Scenario: Invalid command
- **WHEN** the host rejects a client action
- **THEN** the client receives or can derive a rejection and does not treat it as a confirmed world mutation

#### Scenario: Builder disconnects
- **WHEN** a Builder loses connectivity after the match has started
- **THEN** the match continues for the Hunter and other Builders, existing Builder units and towers remain under their original ownership, and no new commands from the lost Builder's session are accepted

#### Scenario: Hunter disconnects
- **WHEN** the assigned Hunter (player 1) loses connectivity during a match
- **THEN** the match ends for everyone; no surviving Builder continues in an independently divergent simulation

#### Scenario: Original Builder lobby creator disconnects
- **WHEN** the original lobby creator was assigned Builder and disconnects after the Hunter authority handoff
- **THEN** the Hunter and remaining Builders continue in the independent Hunter-owned room
- **AND** the departed Builder's session cannot issue further accepted commands

### Requirement: Windows direct-IP multiplayer works

The initial networking implementation SHALL allow connected Windows
clients on reachable addresses/ports to play a common match.

#### Scenario: Two Windows machines
- **WHEN** two compatible Windows clients use the same reachable relay
- **THEN** both clients can observe shared Builder orders, manual Hunter horse attack-move, tower attacks and kill outcomes

#### Scenario: Four or eight concurrent human participants
- **WHEN** four or eight human clients join a room, and the Hunter role is assigned
- **THEN** all Builders receive recipient-private snapshots and can send concurrent orders without changing their session identities
- **AND** a departing Builder cannot terminate the Hunter or another Builder's simulation
- **AND** a departing Hunter ends the match on all remaining clients

### Requirement: Internet participants can use an outbound relay

The networking implementation SHALL support pairing one outbound authoritative host connection with one outbound guest connection while preserving the normal match handshake, session identity and authoritative gameplay protocol.

#### Scenario: Host and guest are behind routers
- **WHEN** the lobby gives the host and guest separate credentials for one player pair and both connect to the relay
- **THEN** the relay forwards their normal handshake and gameplay frames without either participant accepting an inbound Internet connection

#### Scenario: Relay credential has the wrong role or pair
- **WHEN** a peer presents a token that does not authorize its requested host or guest role for that match and player
- **THEN** the relay rejects that connection without consuming either valid side of the pair

#### Scenario: Multiple relay pairs are active
- **WHEN** the lobby provisions multiple host-to-guest pairs and their roles connect in any order
- **THEN** each completed pair forwards independently and a disconnect closes only the affected pair

#### Scenario: Lobby cancels pending relay admission
- **WHEN** the lobby cancels a provisioned pair before both roles arrive
- **THEN** the relay removes the admission and closes any role that already connected
