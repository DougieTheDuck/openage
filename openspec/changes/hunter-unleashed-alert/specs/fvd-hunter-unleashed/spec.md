# Spec Delta

## ADDED Requirements

### Requirement: Hunter unleashed alert

When the countdown to the Hunter's first horse spawn reaches zero, every
player SHALL see a HUNTER UNLEASHED banner for about three seconds and hear
the Hunter unleashed sound, once per match.

#### Scenario: Countdown ends
- **WHEN** the Hunter countdown goes from 1 to 0
- **THEN** all players see the banner and hear the sound once

#### Scenario: Countdown skipped
- **WHEN** the host presses F9 with 14 seconds left
- **THEN** the alert plays immediately

#### Scenario: Late join
- **WHEN** a player's first view of the match is after the horses started
- **THEN** no banner or sound plays
