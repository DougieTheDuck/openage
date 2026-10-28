# Spec Delta

## ADDED Requirements

### Requirement: Hunter wins when all Builders are dead

The match SHALL end with "Hunter wins" as soon as no Builder unit is alive.

#### Scenario: Last Builder dies
- **WHEN** the last living Builder is killed
- **THEN** every player sees the Hunter wins screen and no further orders are accepted

### Requirement: Builders win on time

The match SHALL end with "Builders win" when 45 minutes of match time have
passed and at least one Builder is alive.

#### Scenario: Timer expires
- **WHEN** match time reaches 45:00 with Builders alive
- **THEN** every player sees the Builders win screen

### Requirement: Builders win by tower count

The match SHALL end with "Builders win" as soon as the Builders together have
7000 finished towers.

#### Scenario: 7000th tower
- **WHEN** a tower completes and the Builders' finished towers total 7000
- **THEN** every player sees the Builders win screen

### Requirement: A dead Builder's towers collapse

When a Builder dies, all towers, towers under construction and foundations
owned by that player SHALL be removed, with finished towers playing the
collapse animation.

#### Scenario: Builder killed
- **WHEN** a Builder owning 12 towers is killed
- **THEN** all 12 collapse and their tiles become free
