# Proposal

## Why

A tower currently appears instantly on Ctrl-left-click regardless of where
the Builder is standing. FVD needs real construction orders before its build
menu, tower combat and shared multiplayer match can work reliably.

## What Changes

- Replace instant tower spawning with a simulation-owned BuildTower order.
- Validate the owning Builder, their human player's right to build, and
  the vacant target tile.
- Reserve the tile, move to a reachable adjacent stand tile, then complete
  construction after a finite configurable duration.
- Cancel on new orders, loss of the Builder, unreachable sites or conflicts.
- Keep Ctrl-click as a solo developer shortcut until the build menu is ready.
- Test contention and cancellation without a renderer.

## Capabilities

### New Capabilities

- `fvd-construction`: validated, interruptible, timed Builder construction.

### Modified Capabilities

None; the capability has not yet been established as an archived spec.

## Impact

- `libopenage/gamestate/fvd.*` and FVD simulation tests
- Game commands, event handlers, activity and movement code
- `libopenage/input/controller/game/` for the local shortcut
- `assets/fvd/data/` for construction settings

In multiplayer, player 1 is a **human Hunter**, not an AI player, but is
still forbidden to build. Only assigned Builder slots 2-8 may construct.
Role assignment and networking belong to the later changes.
