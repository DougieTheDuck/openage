# Tasks

## 1. Simulation and ownership

- [x] 1.1 Inspect command/activity sequencing and introduce BuildTower, retaining requesting player, Builder, target tile and order identity.
- [x] 1.2 Reject orders from Hunter player 1 (human in a match), unowned/invalid Builders, occupied or out-of-bounds tiles; cover valid slots 2-8 and invalid cases in tests.
- [x] 1.3 Reserve the target tile and resolve conflicting requests in event-processing order; assert at most one successful claim.

## 2. Movement, completion and interruption

- [x] 2.1 Reuse pathfinding to reach a valid adjacent stand location; test distant, adjacent and unreachable sites.
- [x] 2.2 Begin a positive configurable construction duration after arrival; create exactly one completed tower only at completion.
- [x] 2.3 Cancel and release the reservation on replacement orders, unreachable paths and Builder death; prove stale scheduled events cannot complete.

## 3. Developer flow and checks

- [x] 3.1 Retarget Ctrl-left-click to BuildTower for the existing solo mode (Hunter/Builder hotkey role switching stays available).
- [x] 3.2 Update FVD gameplay documentation to describe travel, completion and cancellation.
- [ ] 3.3 Run focused simulation tests, make tests and available quality checks; visually verify in FVD where a graphics runtime is available.

## Follow-up

- Feed construction state into the QML build-menu/HUD change.
- Archive after implementation and verification, not before.
