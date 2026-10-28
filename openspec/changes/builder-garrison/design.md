# Design notes

- Garrison: the Builder walks to a free stand tile next to the tower (as for
  construction), then is taken off the map. Keep the entity for ownership and
  "alive" checks, but remove its position from vision, targeting, collision and
  pathfinding.
- Replication: the owner sees "garrisoned in tower X"; the Hunter sees nothing
  (it is inside a building).
- Ungarrison point: reuse the rally point code (marker entity per tower,
  owner-only visibility, `closest_reachable` when walled off).
- Adjacent hop: footprints touching, including diagonally. The Builder moves
  instantly, or after a short animation, without walking outside.
- Commands: `GARRISON` (Builder plus tower), `UNGARRISON` (tower),
  `SET_UNGARRISON_POINT` (tower plus point).

## Open questions

- How many Builders fit in one tower? With one Builder per player, 1 is enough,
  but allied Builders sharing a tower needs a decision.
- What happens to a garrisoned Builder when the tower is destroyed (ejected
  unharmed, damaged, or killed)? AoE2 ejects.
- Can a garrisoned Builder still build? Does garrisoning change the tower
  (e.g. more arrows)?
- Does a garrisoned Builder count as alive for "all Builders dead"? Assumed yes.
- Default ungarrison point when none is set: the tile the Builder entered from.
