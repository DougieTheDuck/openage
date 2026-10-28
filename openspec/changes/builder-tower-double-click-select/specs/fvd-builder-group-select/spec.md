# Spec Delta

## ADDED Requirements

### Requirement: Double-click selects visible buildings of the same kind

A Builder player double-clicking one of their finished towers SHALL select all
of their finished towers visible on screen. Double-clicking one of their
foundations SHALL select all of their foundations visible on screen. A
selection SHALL contain only the kind that was double-clicked, never other
players' buildings, and never anything off screen.

#### Scenario: Towers
- **WHEN** a Builder double-clicks a finished tower with five more of their finished towers and two foundations on screen
- **THEN** the six towers are selected and the foundations are not

#### Scenario: Foundations
- **WHEN** a Builder double-clicks a foundation
- **THEN** all of their foundations on screen are selected and no towers

### Requirement: Delete a selected group

Delete SHALL remove every building in a selected group of the player's towers
or foundations, with the same rules as deleting each one singly.

#### Scenario: Group delete
- **WHEN** a Builder presses Delete with six of their towers selected
- **THEN** all six collapse and their tiles become free
