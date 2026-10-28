# Design

`FVD::set_hunter_rally_point(state, owner, ids, point, time)` accepts only
player 1 naming exactly the Hunter tower, an in-bounds ground point, and a point
horses can path to from the spawn tile (which also excludes tower footprints).
It replaces the `HunterRallyPoint` marker entity rather than moving it, so
clients see it appear in its new place. The marker has only an animated Idle
ability: no `Selectable` (so it is not pickable), no `LineOfSight` (so it gives
no vision) and no health.

`spawn_hunter_horse` issues `Move::move_default` to the rally point after the
crowd solver has placed the horse. A travelling horse does not start stance
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
