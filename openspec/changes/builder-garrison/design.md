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

## Decisions (Dougie, 2026-10-28)

- One Builder per tower. A hop into an occupied tower is refused and the
  Builder ungarrisons to the point's closest free tile instead.
- When a tower is destroyed (by horses or Delete) its Builder is ejected
  unharmed onto a free adjacent tile before the collapse starts.
- A garrisoned Builder cannot build: build orders for him are rejected, and
  garrisoning cancels his current job (his queued foundations stay).
- Garrisoning does not change the tower's damage, range, armour or rate of fire.

## Still to decide

- Default ungarrison point when none is set; proposed: the tile the Builder
  entered from.
- A garrisoned Builder counts as alive for "all Builders dead"; assumed yes.
