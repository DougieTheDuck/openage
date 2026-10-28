# Proposal

## Why

Clearing a long queue of planned towers one foundation at a time is slow,
especially after a Shift-drag line goes wrong.

## What Changes

- With one of your Builders selected, the HUD shows **Delete all foundations**.
  It removes every foundation belonging to that Builder: the site he is walking
  to and everything in his queue.
- The tower he is currently building, finished towers and other Builders'
  foundations are untouched.
- The button only appears while that Builder has at least one foundation.
- "Queued towers" drops to zero.

## Capabilities

### Modified Capabilities

- `fvd-builder-construction`: bulk removal of one Builder's planned sites.

## Status

Proposed (Dougie, 2026-10-28). Not implemented.
