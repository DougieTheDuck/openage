# Spec Delta

## ADDED Requirements

### Requirement: Hunter rally point

The Hunter SHALL be able to set a rally point by selecting the Hunter tower and
right-clicking ground that horses can reach. Every horse spawned afterwards
SHALL ride there as though given a Move order, then follow its stance on
arrival. Setting the rally point again SHALL replace it. Without a rally point,
horses SHALL stay where they spawn. Only the Hunter SHALL be able to set it, and
only through the Hunter tower.

#### Scenario: Rally point set
- **WHEN** the Hunter right-clicks reachable ground with the Hunter tower selected
- **THEN** a rally marker appears there and the next horse rides from beside the tower to it

#### Scenario: Invalid rally point
- **WHEN** the rally point is out of bounds, unreachable, inside a footprint, or requested by a Builder or without the Hunter tower
- **THEN** the command is rejected and any existing rally point is unchanged

### Requirement: Rally marker is private to the Hunter

The rally point SHALL be marked by a red arrow pointing at the ground. It SHALL
be shown only to the Hunter, never sent to Builder clients even where they have
vision, and SHALL NOT be selectable.

#### Scenario: Builder beside the marker
- **WHEN** a Builder stands next to the rally point
- **THEN** the Builder's world state does not contain the marker

### Requirement: Horse groups ignore the Hunter tower

A box selection containing horses SHALL select only the horses. A horse order
that also names the Hunter tower SHALL apply to the horses and ignore the tower.

#### Scenario: Tower swept up in a box
- **WHEN** the Hunter box-selects the tower with several horses and right-clicks ground
- **THEN** the horses move and the order is not rejected
