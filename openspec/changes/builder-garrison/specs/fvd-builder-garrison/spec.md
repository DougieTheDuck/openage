# Spec Delta

## ADDED Requirements

### Requirement: Garrison in own towers

A Builder player SHALL be able to press G with their Builder selected and click
one of their finished towers to send the Builder inside it. A garrisoned Builder
SHALL be hidden from other players and SHALL NOT be targeted or damaged. Only
the owner's finished towers SHALL accept a garrison.

#### Scenario: Garrison
- **WHEN** a Builder presses G and clicks their own finished tower
- **THEN** the Builder walks to it and disappears inside; horses can no longer target him

#### Scenario: Invalid tower
- **WHEN** the clicked tower is someone else's, unfinished, already occupied, or the Hunter tower
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
no further order, even if enemies are near. The owner MAY set an ungarrison
point by selecting the tower and right-clicking; it SHALL be marked with the
same red arrow as the Hunter's rally point and shown only to the owner. With a
point set, the ungarrisoned Builder SHALL walk to it, or as close as he can get.

#### Scenario: Ungarrison without a point
- **WHEN** the owner presses the ungarrison key on a tower with no ungarrison point while a horse stands nearby
- **THEN** the Builder appears on a free tile beside the tower, stays there and can be attacked

#### Scenario: Ungarrison with a point
- **WHEN** the owner has set an ungarrison point and presses the ungarrison key
- **THEN** the Builder appears beside the tower and walks to the red arrow

### Requirement: Hop between adjacent towers

When a tower's ungarrison point is on another finished, empty tower of the same
owner whose footprint touches it, ungarrisoning SHALL move the Builder directly
into that tower without exposing him outside.

#### Scenario: Hop
- **WHEN** towers A and B are adjacent, A's ungarrison point is on B, and the Builder ungarrisons from A
- **THEN** the Builder is garrisoned in B and was never targetable in between
