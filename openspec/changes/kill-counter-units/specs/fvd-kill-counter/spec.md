# Spec Delta

## MODIFIED Requirements

### Requirement: Builder kill counters count enemy units killed

A Builder's kill counters SHALL count enemy units killed by that player's
towers, and SHALL be labelled as units killed. Destroyed towers SHALL never
change them.

#### Scenario: Tower kills a horse
- **WHEN** a player's tower lands the killing arrow on a horse
- **THEN** that tower's Units killed and the player's Units killed each rise by one

#### Scenario: Tower destroyed
- **WHEN** horses destroy one of the player's towers
- **THEN** neither counter changes
