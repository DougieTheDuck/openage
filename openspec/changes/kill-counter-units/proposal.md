# Proposal

## Why

Players read the Builder HUD's "Tower kills" as the number of towers destroyed.
What players want to see is how many enemy units their towers have killed.

## What Changes

- Make every Builder kill counter count **enemy units killed** and say so:
  a selected tower shows "Units killed: N" (by that tower), and a selected
  Builder shows "Units killed: N" (by all of that player's towers).
- Confirm in play that the numbers go up when a tower's arrow kills a horse and
  never when a tower is destroyed.

## Current behaviour (2026-10-28)

`FVD::arrow_hit` already credits a kill only when a tower's arrow kills its
target (a horse): it increments the player's total and the firing tower's
count. Nothing counts destroyed towers. So the counting may already be right
and the label is what misleads; if the in-game numbers disagree, that is a bug
to find with a regression test.

## Capabilities

### Modified Capabilities

- `fvd-hunter-combat`: kill counter wording and verification.

## Status

Proposed (Dougie, 2026-10-28). Not implemented.
