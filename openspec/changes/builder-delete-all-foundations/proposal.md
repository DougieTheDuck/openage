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
- The button only appears while that Builder has at least one foundation, and
  shows how many it will delete.
- "Queued towers" drops to zero.

## Capabilities

### Modified Capabilities

- `fvd-builder-construction`: bulk removal of one Builder's planned sites.

## Status

Implemented (Dougie, 2026-11-03). Protocol 22 (shared with `builder-flare`).
