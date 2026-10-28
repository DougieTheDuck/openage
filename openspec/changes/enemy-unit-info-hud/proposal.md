# Proposal

## Why

Builders cannot inspect the enemy: clicking a horse gives no information about
how dangerous it is or how hurt.

## What Changes

Clicking a visible enemy shows in the HUD, for Builders and the Hunter alike:

- **Type/name** (e.g. "Hunter horse", "Hunter tower", "Builder", "Tower")
- **HP** current / total
- **Attack** power and type (e.g. "Melee 3", "Missile 100, 10 tiles",
  "Arrows 2, 7 tiles"; "None" for a Builder)

The selection is read-only: no orders can be given to it. Only enemies your
side can currently see can be inspected (fog of war); the info disappears when
it moves out of sight. A remembered building in explored fog shows only its
type.

## Capabilities

### Modified Capabilities

- `fvd-build-menu-hud`: enemy inspection panel.

## Status

Proposed (Dougie, 2026-10-28). Not implemented.
