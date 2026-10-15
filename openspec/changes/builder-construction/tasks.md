# Tasks

## 1. Construction commands and validation

- [ ] 1.1 Trace FVD's command/activity lifecycle and add a BuildTower order that preserves requesting player and Builder identity; verify an owned Builder can issue it in a headless test.
- [ ] 1.2 Implement buildable tile, ownership, occupancy and reservation validation; verify negative and competing-order cases in `fvd_scenario` regression tests.
- [ ] 1.3 Centralize order cancellation and reservation cleanup; verify replacement orders leave no reserved tile or later tower.

## 2. Travel and timed completion

- [ ] 2.1 Find a reachable stand tile adjacent to the proposed footprint using existing pathfinding; verify a distant Builder travels without entering the build tile, and unreachable locations fail cleanly.
- [ ] 2.2 Start a configurable positive-duration construction after arrival and schedule completion in simulation time; verify the tower cannot exist as completed before the deadline.
- [ ] 2.3 Revalidate the order and site at completion; verify a canceled, interrupted or superseded order never creates a tower and a successful order creates exactly one.

## 3. Existing controls and integration

- [ ] 3.1 Point Ctrl-left-click at the build-order command and keep Hunter/foreign-unit restrictions; verify the manual local workflow in an FVD window.
- [ ] 3.2 Update FVD gameplay documentation for travel, construction and cancellation; verify the stated shortcut and behavior match the runnable prototype.
- [ ] 3.3 Run `make tests` and `make checkmerge` (or record environment blockers) and verify the FVD game visually where a graphical build is available.

## Workflow follow-up

- Review completion behavior before adding QML construction progress.
- Archive this change once its implementation and checks are accepted.
