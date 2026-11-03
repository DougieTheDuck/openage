# Spec Delta

## ADDED Requirements

### Requirement: Minimap

The HUD SHALL show a minimap at the bottom left, with the build panel directly
to its right. It SHALL show the viewer's fog of war, buildings in their
owner's colour and units as dots in their owner's colour, using only state
the viewer is allowed to see, and a box for the current camera view. Clicking
or dragging on it SHALL move the camera to that point.

#### Scenario: Fog respected
- **WHEN** a Builder's tower stands in the Hunter's unexplored area
- **THEN** the Hunter's minimap shows black there, not the tower

#### Scenario: Navigate
- **WHEN** a player clicks the minimap's top corner
- **THEN** the camera centres on that part of the map and the box follows

#### Scenario: Building colours
- **WHEN** player 3 completes a tower in a visible area
- **THEN** it appears on everyone's minimap who can see it, in player 3's colour
