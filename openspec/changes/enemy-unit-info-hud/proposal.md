# Proposal

## Why

Builders cannot inspect the enemy: clicking a horse gives no information about
how dangerous it is or how hurt.

## What Changes

As a Builder, clicking a visible enemy unit shows in the HUD:

- **Type/name** (e.g. "Hunter horse", "Hunter tower")
- **HP** current / total
- **Attack** power and type (e.g. "Melee 3", "Missile 100, 10 tiles")

The selection is read-only: no orders can be given to it. Only units the
Builders can currently see can be inspected (fog of war); the info disappears
when it moves out of sight.

## Capabilities

### Modified Capabilities

- `fvd-build-menu-hud`: enemy inspection panel.

## Status

Proposed (Dougie, 2026-10-28). Not implemented.
