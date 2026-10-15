# Design

## Context

`FVD::place_tower` currently validates then spawns immediately. The
Ctrl-click controller sends `game.spawn_entity`; `fvd_test.cpp` checks
instant placement. Pathfinding, Move commands, queued events and
time-indexed positions already exist.

## Goals / Non-Goals

**Goals:** ordered movement, ownership and tile validation, stand-tile
selection, reservation, positive construction duration, cancellation and
headless simulation behavior.

**Non-Goals:** final build UI, resources, multiple workers per site, combat,
network transport, animation polish or Hunter unit controls.

## Decisions

### Build orders use the command lifecycle

Represent construction as a new Builder action, not a rendering callback or
unconditional spawn. Reuse the event loop and existing command system.
The command carries the Builder identity and a claimed target tile.
Check ownership against the current match's assigned human player; player 1
is ineligible whether controlled by a solo tester or a multiplayer human.
Keep Ctrl-click as the solo test interface, and make future QML/network
requests call the same authoritative code path.

### The Builder must physically reach the site

Find a reachable, unoccupied tile adjacent to the footprint with existing
pathfinding. Begin a finite, data-driven build duration only after arrival.
Do not base completion on wall-clock or render-frame time.

### Prevent double occupancy and stale completion

Keep reservations in simulation state, distinct from complete towers.
Reserve atomically when the order is accepted, release on failure or
cancellation, and consume the reservation when the tower completes.
Give orders stable identities; at completion, verify the Builder is still
alive, on that order and at a valid site.

### Interrupt consistently

A replacement move/build order or the Builder's destruction cancels
pending work. Ensure scheduled arrival/completion events cannot spawn an
orphan tower after cancellation. Preserve curve semantics when movement is
interrupted.

## Risks / Trade-offs

- Pathfinder terrain occupancy may not account for temporary reservations;
  validate candidate stand tiles against actual game state.
- Movement curves can schedule future positions; verify that replacing an
  order makes prior waypoints and completion events harmless.
- Combat later needs to call the same cancellation cleanup when a Builder
  dies.
- Progress feedback belongs to the later build-menu/HUD feature.

## Migration Plan

Change the Ctrl-click action from immediate tower spawn to BuildTower,
update the FVD test to assert travel and completion timing, and leave the
existing map/role setup usable in solo development mode.
