# Design

## Context

FVD already has sprite data for horses and towers and simulation primitives
for units, ownership, movement and health. The current Tower definition
does not yet attack, and the Hunter begins with no units.

## Proposed Approach

Schedule one horse spawn per second of game simulation time after match start,
owned by player 1, from the center tile/position. Use the engine's event loop
and pathfinding for spawning/movement; do not build a per-frame timer.
Use a target-selection and attack event for completed towers, with explicit
arrow shots, damage and a single death transition. Credit the lethal hit
to the tower's owner in authoritative match state, not in client QML.
Avoid duplicate credit when projectiles overlap or events share a timestamp.

The human Hunter controls the Hunter role. The roadmap retains the phrase
"Player 1 AI spawns and steers" while also specifying random human selection.
Treat it as a request for automatic *unit behavior* until clarified, not as
an AI-controlled player.

## Open Decisions

- What direct commands, if any, can the human Hunter issue to horses?
- What target or route do automatically moving horses use on the initially
  open 128x128 map, and do they damage Builders/towers in this milestone?
- Exact tower range, rate of fire, damage, projectile travel time and
  horse health should be configurable and agreed before implementation.

These questions must not block implementing deterministic spawn cadence,
tower damage, kill credit or a test harness.
