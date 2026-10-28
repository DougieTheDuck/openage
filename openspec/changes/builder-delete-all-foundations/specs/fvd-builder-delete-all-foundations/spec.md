# Spec Delta

## ADDED Requirements

### Requirement: Delete all of a Builder's foundations

With exactly one of their Builders selected, a Builder player SHALL be able to
remove every foundation belonging to that Builder in one action. A tower under
construction, completed towers and other players' foundations SHALL NOT be
affected. The action SHALL only be offered while the Builder has at least one
foundation.

#### Scenario: Queue cleared
- **WHEN** a Builder walking to one site with three more queued uses Delete all foundations
- **THEN** all four foundations disappear, their tiles are free, the Builder stops, and Queued towers shows 0

#### Scenario: Building in progress
- **WHEN** the Builder is constructing a tower and has two queued sites
- **THEN** the two queued foundations are removed and construction continues

#### Scenario: Someone else's Builder
- **WHEN** a player sends the command for another player's Builder
- **THEN** the host rejects it and nothing changes
