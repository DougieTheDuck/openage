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
- **WHEN** the clicked tower is someone else's, unfinished, or the Hunter tower
- **THEN** the order is rejected

### Requirement: Ungarrison point and shortcut

Each tower SHALL have an ungarrison point that its owner can set by selecting
the tower and right-clicking; it SHALL be shown only to the owner. A shortcut
key SHALL ungarrison the Builder, who then moves to the ungarrison point (or as
close as he can get).

#### Scenario: Ungarrison
- **WHEN** the owner selects a garrisoned tower and presses the ungarrison key
- **THEN** the Builder reappears next to the tower and walks to its ungarrison point

### Requirement: Hop between adjacent towers

When a tower's ungarrison point is on another finished tower of the same owner
whose footprint touches it, ungarrisoning SHALL move the Builder directly into
that tower without exposing him outside.

#### Scenario: Hop
- **WHEN** towers A and B are adjacent, A's ungarrison point is on B, and the Builder ungarrisons from A
- **THEN** the Builder is garrisoned in B and was never targetable in between
