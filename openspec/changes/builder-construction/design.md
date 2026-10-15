# Design

## Context

In `FVD::place_tower`, validation and `spawn_game_entity` currently happen
in one call. The FVD input binding sends `game.spawn_entity` directly on
Ctrl-left-click. The engine already has Move commands, the pathfinder and
time-indexed position curves, while event handlers can schedule game actions.
`FVD::place_tower` is exercised in `fvd_test.cpp`.

## Goals / Non-Goals

**Goals:** command-driven movement to a valid stand tile, collision-safe site
reservation, timed completion, ownership enforcement and cancellation,
including when run without a renderer.

**Non-Goals:** build menu, resources, multiple Builder cooperation, combat,
animation polish, networking or upgrading towers.

## Decisions

### 1. Build is a command, not a special spawn click

Introduce a FVD BuildTower command/order (or extend the existing command
model) and submit it through the event loop, not directly through a renderer
callback. Keep Ctrl-left-click mapped to this order for development.
Use the same authoritative validation for future QML buttons and network
commands. Do not add construction logic to the input controller.

### 2. Reuse the engine's movement and event machinery

Use existing pathfinding to identify a valid adjacent stand location.
Coordinate movement with the active order, then schedule construction
completion relative to simulation time. Avoid per-render-frame timers or a
parallel movement implementation. Revalidate the Builder and order when a
scheduled event runs; stale scheduled completions must be harmless.

### 3. Reserve tiles in simulation-owned state

Track which incomplete order owns each reserved tile, separately from
completed tower occupation. Validate against units, completed towers and
existing reservations. Release reservations on cancellation/failure; clear
them atomically on successful completion. If two requests compete, event
processing order decides the winner.

### 4. Keep the first build duration data-driven

Use one positive construction duration in FVD scenario/ability data,
rather than a UI timer or hardcoded wall-clock delay. The current tower
sprite may be used as a temporary construction marker if helpful, but a
completed tower must not exist before construction finishes.

### 5. Integrate interruption with the Builder's command lifecycle

A replacement move/build command cancels a pending build. On death or
unreachable route, clear the same order state. Ensure a subsequent
completion event checks a stable order identity, so a canceled order
cannot finish accidentally after a later order starts.

## Risks / Trade-offs

- **Pathfinding currently considers terrain, not necessarily temporary tower
  reservations:** select/validate a destination adjacent to the footprint;
  confirm occupancy assumptions with integration tests.
- **Future combat may destroy Builders:** keep cancellation centralized so
  the death handler can invoke it rather than duplicating cleanup.
- **Movement is recorded on curves ahead of time:** command interruption
  may require trimming or overriding scheduled movement and arrival.
  Investigate the activity system before choosing exactly where orders live.
- **Visible construction may precede a dedicated HUD:** use clear diagnostic
  state and tests now; let the later HUD change display progress.

## Migration Plan

Update the current Ctrl-click binding to issue BuildTower instead of directly
spawning a tower, then adjust the FVD scenario test from instant placement
assertions to order/arrival/completion assertions. Keep existing FVD map and
player setup intact. Change only the relevant FVD behavior.
