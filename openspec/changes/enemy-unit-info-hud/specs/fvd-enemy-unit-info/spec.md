# Spec Delta

## ADDED Requirements

### Requirement: Inspect a visible enemy unit

A Builder player SHALL be able to click a visible enemy unit to see its type,
current and maximum HP, and its attack power and type in the HUD. The selection
SHALL accept no orders. Enemy units the Builders cannot see SHALL NOT be
selectable, and the panel SHALL clear when the unit leaves view.

#### Scenario: Inspect a horse
- **WHEN** a Builder clicks a visible Hunter horse
- **THEN** the HUD shows "Hunter horse", its HP as current/total and its melee attack

#### Scenario: No orders
- **WHEN** a Builder right-clicks the ground with an enemy horse selected
- **THEN** no command is sent

#### Scenario: Leaves view
- **WHEN** the inspected horse moves into fog
- **THEN** the selection clears
