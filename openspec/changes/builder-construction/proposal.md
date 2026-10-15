# Proposal

## Why

A tower currently appears instantly when a user Ctrl-clicks a vacant tile,
even if their Builder is on the other side of the map. FVD needs building
orders to behave as real gameplay actions before adding the build menu,
combat and multiplayer synchronization.

## What Changes

- Replace FVD's instant tower spawn with a Builder construction order.
- Validate the controlling player, chosen Builder and requested tile.
- Reserve an available tower tile while the Builder travels to a reachable
  adjacent position, then complete construction after a finite duration.
- Cancel and clean up work when the Builder is redirected, lost or blocked.
- Preserve a Ctrl-click testing path until the build menu is available.
- Add regression tests covering valid and invalid orders, timing and conflicts.

## Capabilities

### New Capabilities

- `fvd-construction`: Builder-owned construction orders, adjacency,
  reservation, timed completion and cancellation.

### Modified Capabilities

None; no existing OpenSpec capability is being modified.

## Impact

- `libopenage/gamestate/fvd.*` and `libopenage/gamestate/fvd_test.cpp`
- `libopenage/gamestate/event/`, command queue and activity/movement systems
- `libopenage/input/controller/game/` for local command submission
- `assets/fvd/data/` for configurable construction parameters if needed

No networking protocol, resource economy or final build-menu UI is included.
