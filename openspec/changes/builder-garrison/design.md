# Design notes

- Garrison: the Builder walks to a free stand tile next to the tower (as for
  construction), then is taken off the map. Keep the entity for ownership and
  "alive" checks, but remove its position from vision, targeting, collision and
  pathfinding.
- Replication: all viewers with vision of the tower see the occupying
  Builder's player-colour flag, even when an ally built the tower. The
  concealed Builder's ID stays on the host for ALL recipients, including its
  owner: G targets the tower ID, and the host enforces the Builder's ownership.
  This avoids leaking or mismatching private identities in delta snapshots.
- Ungarrison point: one independent marker per (tower, player), using
  the same `hunter_rally_point` red arrow sprite, visible only to that player
  while the source tower is selected,
  `closest_reachable` when walled off). It is optional: without one, the
  Builder is placed on the free adjacent tile and given no order.
- Adjacent hop: footprints touching, including diagonally. The Builder moves
  instantly, or after a short animation, without walking outside.
- Commands: `GARRISON` (Builder plus any allied tower), `UNGARRISON`
  (tower containing the caller's own Builder only), `SET_UNGARRISON_POINT`
  (allied tower plus per-player point/optional target tower ID).

## Decisions (Dougie, 2026-10-28)

- One Builder per tower, regardless of which ally owns it. A hop into an
  occupied tower is refused and its Builder remains sheltered.
- When a tower is destroyed (by horses or Delete) its Builder is ejected
  unharmed onto a free adjacent tile before the collapse starts.
- A garrisoned Builder cannot build: build orders for him are rejected, and
  garrisoning cancels his current job (his queued foundations stay).
- Garrisoning does not change the tower's damage, range, armour or rate of fire.

- No ungarrison point: the Builder steps straight out onto a free tile next
  to the tower and stays, exposed to attack (Dougie, 2026-10-28). Tie-break
  between free tiles deterministically (e.g. the tile he entered from if free,
  otherwise a fixed scan order).

## Still to decide

- A garrisoned Builder counts as alive for "all Builders dead"; assumed yes.
