# Spec Delta

## ADDED Requirements

### Requirement: Lobby accepts two to eight human participants

The game SHALL allow a host and other humans to join a lobby using a direct
IP address and port on Windows, with 2-8 total human participants to start.

#### Scenario: Second participant joins
- **WHEN** a guest connects to a compatible lobby with room
- **THEN** the guest appears in the participant list

#### Scenario: Lobby is full
- **WHEN** an additional guest attempts to join an eight-human lobby
- **THEN** that connection is rejected without replacing a participant

#### Scenario: Incompatible client
- **WHEN** a client with an incompatible required game/protocol version joins
- **THEN** the client is rejected with a reason

### Requirement: Relay rooms are discoverable

The game SHALL let a host advertise a named room on a configured relay and let guests list and join waiting rooms without administrator-provisioned pair identifiers.

#### Scenario: Public host advertises a room
- **WHEN** a host creates a room on a relay with public access
- **THEN** the relay returns a nonzero room ID and includes the room in subsequent listings

#### Scenario: Guest joins a listed room
- **WHEN** a guest joins an available room ID
- **THEN** the relay removes the room from its listing and pairs the guest with its host

#### Scenario: Private alpha restricts directory access
- **WHEN** the relay starts with an authorized player-token list
- **THEN** room creation, listing and joining reject tokens outside that list

### Requirement: The host initiates match start

Only the lobby host SHALL initiate Start; a match SHALL have at least two
human players. A readiness vote SHALL NOT be required.

#### Scenario: Host starts a viable lobby
- **WHEN** the host activates Start with 2-8 connected humans
- **THEN** the game assigns roles and initiates the shared match

#### Scenario: Guest tries to start
- **WHEN** a guest attempts to start the match
- **THEN** the request is rejected

#### Scenario: Host is alone
- **WHEN** the host activates Start with fewer than two humans
- **THEN** no multiplayer match begins

### Requirement: Exactly one human is randomly assigned Hunter

The host SHALL randomly assign exactly one connected human to Hunter
player ID 1 when starting. The host SHALL have Hunter draw weight 0.5
and each guest weight 1.

#### Scenario: Weighted two-human lobby
- **WHEN** the host starts with one guest
- **THEN** the Hunter probabilities are 1/3 host and 2/3 guest

#### Scenario: Weighted eight-human lobby
- **WHEN** the host starts with seven guests
- **THEN** the Hunter probabilities are 1/15 host and 2/15 per guest

#### Scenario: Chosen Hunter joins the match
- **WHEN** role assignment finishes
- **THEN** the selected human controls player 1, not an AI player, cannot construct, and can select and command Hunter-owned horses

### Requirement: Every non-Hunter owns a unique Builder slot

The simulation SHALL assign every remaining human a distinct Builder
player ID from slots 2-8, with no human sharing an ID.

#### Scenario: Four people start
- **WHEN** a four-human lobby starts
- **THEN** one human owns Hunter slot 1 and three different humans own Builder slots 2-4

#### Scenario: Online player attempts role switching
- **WHEN** a participant presses the solo role-switch hotkey during a multiplayer match
- **THEN** it cannot change that participant's assigned role

### Requirement: Solo mode is not a multiplayer lobby

The local test mode SHALL allow one tester to switch between player 1
Hunter and player 2 Builder without an AI-controlled player.

#### Scenario: Tester switches role
- **WHEN** solo testing is active and the user presses the role-switch hotkey
- **THEN** local control changes between slots 1 and 2
