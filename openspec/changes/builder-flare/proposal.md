# Proposal

## Why

Builders need a quick way to point teammates at a spot ("help here", "Hunter
coming this way") without typing. AoE2 players use flares for this.

## What Changes

- Builders get a **Flare (F)** button in the build panel. After pressing it,
  the next left-click on the map (or the minimap, once it exists) flares that
  spot. Esc or right-click cancels.
- A flare is a pulsing **star** in the flaring player's colour, drawn on the
  map for 5 seconds, and on the minimap.
- Every Builder hears a short **"gling"** (`flare` sound cue) wherever they are
  looking: it is a call for attention, so it is not culled to the screen.
- **Only Builders** see or hear flares. The Hunter never receives them.
- A player can flare at most once every 2 seconds.

## Capabilities

### New Capabilities

- `fvd-builder-flare`: team map pings for Builders.

## Status

Proposed (Dougie, 2026-11-03). Not implemented. Needs a protocol bump.
