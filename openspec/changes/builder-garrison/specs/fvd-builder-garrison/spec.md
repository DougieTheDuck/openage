# Spec Delta

## ADDED Requirements

### Requirement: Garrison in allied towers

A Builder player SHALL be able to press T with their Builder selected and click
any finished friendly Builder tower to send the Builder inside it. A garrisoned
Builder SHALL be hidden from other players and SHALL NOT be targeted or damaged.
Exactly one Builder, of any friendly player, SHALL occupy each tower.

#### Scenario: Garrison
- **WHEN** a Builder presses T and clicks a friendly finished tower
- **THEN** the Builder walks to it and disappears inside; horses can no longer target him

#### Scenario: Invalid tower
- **WHEN** the clicked tower is unfinished, already occupied, a Hunter tower, or not friendly
- **THEN** the order is rejected

### Requirement: One Builder per tower, no building inside

A tower SHALL hold at most one Builder. A garrisoned Builder SHALL NOT build:
build orders for him SHALL be rejected, and garrisoning SHALL cancel his
current construction job while keeping his queued foundations. Garrisoning
SHALL NOT change the tower's attack, range, armour or rate of fire.

#### Scenario: Build while inside
- **WHEN** a player orders their garrisoned Builder to build a tower
- **THEN** the order is rejected

### Requirement: Destroyed tower ejects its Builder

When a garrisoned tower is destroyed, by horses or by Delete, its Builder SHALL
be placed unharmed on a free tile next to it before the tower collapses.

#### Scenario: Tower falls
- **WHEN** horses destroy a tower with a Builder inside
- **THEN** the Builder appears beside the rubble with the same HP he had inside

### Requirement: Ungarrison and optional ungarrison point

A shortcut key SHALL ungarrison the Builder. Without an ungarrison point, the
Builder SHALL be placed immediately on a free tile next to the tower and given
no further order, even if enemies are near. Every Builder player MAY set a separate ungarrison point on the same allied
tower by right-clicking. It SHALL be marked with the same red arrow as the
Hunter's rally point and shown only to that player while the source tower
is selected. With a
point set, the ungarrisoned Builder SHALL walk to it, or as close as he can get.

#### Scenario: Ungarrison without a point
- **WHEN** the owner presses the ungarrison key on a tower with no ungarrison point while a horse stands nearby
- **THEN** the Builder appears on a free tile beside the tower, stays there and can be attacked

#### Scenario: Ungarrison with a point
- **WHEN** the owner has set an ungarrison point and presses the ungarrison key
- **THEN** the Builder appears beside the tower and walks to the red arrow

### Requirement: Hop between adjacent towers

When a player's ungarrison point targets any adjacent finished, empty allied
tower whose footprint touches the source, ungarrisoning SHALL move the Builder directly
into that tower without exposing him outside.

#### Scenario: Hop
- **WHEN** towers A and B are adjacent, A's ungarrison point is on B, and the Builder ungarrisons from A
- **THEN** the Builder is garrisoned in B and was never targetable in between

### Requirement: Public occupant flag and private command authority

An occupied friendly tower SHALL display the player-colour flag of the
sheltered Builder (not the tower's construction owner) to every player
who currently sees the tower. The concealed Builder's entity ID SHALL be
replicated only to that Builder's player; the flag alone is public.

#### Scenario: Visiting allied tower
- **WHEN** player 2 garrisons inside a finished tower built by player 3
- **THEN** both players see player 2's flag but only player 2 may press G to eject

#### Scenario: Unauthorized ungarrison
- **WHEN** a player presses G on a tower displaying another player's flag
- **THEN** the Builder stays sheltered and the player sees the exact error
  "Can only ungarrison your own builder."

### Requirement: Per-player exit plans

Each (tower, Builder player) SHALL have its own independent ungarrison
destination and optional target-tower identity. Updating one player's
point SHALL NOT overwrite another's. Only the requesting player sees
their red marker while selecting its source tower.

#### Scenario: Shared tower plans
- **WHEN** two Builder players set different exits on the same allied tower
- **THEN** both plans persist and the Builder uses only their player's plan

#### Scenario: Tower destruction
- **WHEN** a tower with multiple stored exit plans is destroyed
- **THEN** all associated private markers and plans are removed
