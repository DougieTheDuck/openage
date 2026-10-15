# Design

## Context

FVD already has sprite data for horses and towers and simulation primitives
for units, ownership, movement and health. The Tower currently cannot attack,
and player 1 begins without units. The existing Move command and selection
system provide a starting point for human Hunter control.

## Proposed Approach

### Give Builders a head start

Use simulation-relative time, with match start as time zero. Schedule the
first horse spawn for **t = 20 seconds**, then t = 21, 22, 23, and so forth:
one additional horse per second while the match is active. No horse can
spawn before 20 seconds, including when a game runs headlessly. The start
delay and cadence should be data-driven (defaults: 20 and 1 seconds).
Do not count lobby time toward the 20-second grace period, and do not use
renderer or wall-clock timers.

### Spawn idle horses at map center

Spawn each horse owned by player 1 at the map center. After spawning,
horses remain idle and may accumulate at the center; do not assign
automatic paths, attack orders, gather locations or rally points.
Check the interaction of overlapped spawning units with occupancy and
pathfinding without routing them away prematurely.

### Human Hunter selection and commands

The multiplayer participant assigned player 1 can select one or more
living, owned horses. Integrate an explicit AttackMove action with the
existing input, command and pathfinding systems. AttackMove orders
specify a destination; horses move towards it and engage eligible
Builder-owned enemies encountered along the way. After a target dies or
becomes invalid, a surviving horse resumes its order toward the
original destination, then waits for another order. Normal Move orders
travel without seeking enemies. None of these orders is issued on spawn.

A selection with several horses issues an order to each selected unit.
The authoritative simulation validates ownership; a Builder must never
be able to move or attack-move a Hunter horse. The solo role-switch mode
uses the same player-1 rules as a multiplayer Hunter assignment.

### Tower combat and kill attribution

Use an event-driven attack for completed towers with an arrow shot,
damage and one definitive death transition per horse. Only credit a
Hunter kill to the owning Builder of the tower dealing lethal damage.
Simultaneous projectiles/events must not double count a death.
The HUD displays only the local player's credited total.

## Open Decisions

- Exact horse/tower damage, attack ranges/rates, projectile travel time,
  targeting priorities and health are balance parameters to establish
  before their final implementation.
- Determine the best occupancy approach for several idle horses standing
  at one spawn point while retaining the requirement not to move until
  given a human order.
- Map the human AttackMove action into the current controller UX with a
  minimal shortcut; a complete Hunter control panel is not required.

The 20-second grace period, one-per-second cadence, manual orders and lack
of a gather/rally point are fixed product requirements.
