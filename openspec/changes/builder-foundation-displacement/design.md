# Design: Occupied foundation clearance

## Authoritative state

`FVD::can_place_tower` separates static structure/marker reservations from
temporary live-unit occupancy. A valid queued site always has a concrete
Foundation entity and a deterministic queue position.

`PendingBuild::blocked_since` marks the start of a five-second wait once the
active job's arrival event discovers that its footprint or stand is blocked.
The normal construction-time timer begins *only* after the Builder reaches an
adjacent stand and the footprint has cleared.

## Displacement

At each 250 ms scheduled clearance check, collect non-garrisoned Builder and
Hunter Horse entities on the footprint, sorted by entity ID. Exclude the
building Builder from generic shoving; it follows the usual construction-stand
route. Other units are given a reachable, passable, unoccupied destination
outside the foundation footprint. Avoid reservations and reusing exit tiles.
Leave units already travelling out of the footprint alone.

Do not teleport units or manufacture a successful path when the route is
blocked. Five seconds without clearance is a deterministic failure.

## Timeout and queue continuity

On timeout, release only the active site's reservations, remove its Foundation
entity, discard its pending build and start the next queued foundation.
Do not call `cancel_build` for this case: that routine intentionally clears
the entire future queue when the user issues a replacement command.

Other deliberate command cancellations and Builder deaths retain the existing
cancellation semantics. Route-revision and order-identity checks prevent stale
scheduled events from resuming a skipped foundation.

## Testing

Test a foundation under the building Builder, an occupied queued site with
another Builder, a trapped Hunter Horse causing a five-second timeout, cleanup
of its Foundation and reservation, and continued FIFO construction. Re-run
existing construction/drag-queue/prioritization tests plus multiplayer
replication checks.
