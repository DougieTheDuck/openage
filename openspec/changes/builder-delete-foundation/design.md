# Design

`FVD::demolish_owned_tower` hands a `Foundation` ID to
`FVD::delete_owned_foundation(state, owner, id, time)`, which requires a Builder
player owning the foundation and then:

1. removes it from whichever Builder's queue holds it, releasing that site's
   reservations;
2. if it is the active, not yet started job (`build_orders[b].foundation_id`
   without a tower), moves the rest of that Builder's queue aside, cancels the
   job (which would otherwise clear the queue), restores the queue and, after
   removing the foundation, calls `start_next_queued_build` (the code that runs
   after a tower is finished, extracted for reuse);
3. removes the foundation entity.

Construction removes a job's foundation when building starts, so a started
job is a tower and keeps the tower path.

Client side, a lone foundation selects as `SelectionKind::FOUNDATION` (locally
and on guests), Delete accepts `TOWER` or `FOUNDATION`, the HUD labels it, and
the stale-selection cleanup that already handled towers also clears a deleted
foundation. `units_take_priority` (formerly horse-only) keeps the horses or
Builders of a box when there are any.
