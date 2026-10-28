# Design

`FVD::set_hunter_rally_point(state, owner, ids, point, time)` accepts only
player 1 naming exactly the Hunter tower and an in-bounds ground point; whether
horses can reach it is decided per spawn, because walls change during a match.
It replaces the `HunterRallyPoint` marker entity rather than moving it, so
clients see it appear in its new place. The marker has only an animated Idle
ability: no `Selectable` (so it is not pickable), no `LineOfSight` (so it gives
no vision) and no health.

`spawn_hunter_horse` issues `Move::move_default` to `closest_reachable(...)`
after the crowd solver has placed the horse: the rally point itself when the
pathfinder finds a route, otherwise the tile nearest it among those reachable
from the horse (a fixed-order orthogonal flood fill over the land cost field,
so the result is deterministic and pathable). A horse that cannot move at all
stays put.

`Pathfinder::get_path` used to follow the portal A* "closest portal" fallback
when no route existed. That partial route starts on the start side and need not
touch the target sector, so the flow-field walk asserted `Invalid entry sector`
and threw. Every caller ignores non-FOUND paths, so `get_path` now returns
`NOT_FOUND` with no waypoints as soon as the portal search fails. A travelling horse does not start stance
engagements, so it reaches the rally point first and then behaves by stance,
exactly as after a human Move.

Replication drops the marker for every Builder recipient in
`capture_fvd_state`, and the local presentation view drops it when a Builder
is controlled. Guests render `HunterTower` and `HunterRallyPoint` (the guest
whitelist previously omitted the Hunter tower), and the guest marker proxy is
not pickable.

Selections: `horses_take_priority` keeps only the horses of a multi-entity box
when there are any. A lone Hunter tower selects as `SelectionKind::HUNTER_TOWER`,
and right-click then sends `SET_RALLY_POINT`, locally or as a guest.
`resolve_command_units` skips the Hunter tower for owner 1, like a dead horse,
so an order naming it plus horses applies to the horses; the tower alone is
still rejected.
