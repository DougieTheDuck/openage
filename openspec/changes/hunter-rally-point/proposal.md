# Proposal

## Why

New Hunter horses stand beside the central tower until ordered. Like an AoE2
rally point, the Hunter should be able to choose where new horses gather
without ordering every wave by hand. Separately, a box selection that sweeps up
the tower with the horses currently cannot be ordered at all, because the host
rejects any horse order naming a non-horse.

## What Changes

- With the Hunter tower selected, right-click sets its **rally point**. Every
  horse spawned afterwards rides there as though given a Move order. Setting it
  again moves it; until one is set, horses stay where they spawn.
- As in AoE2, it may be set anywhere on the map. When Builders have walled in
  the centre, or built over the rally point, new horses ride to the reachable
  tile closest to it instead of standing still.
- Fix an openage pathfinder crash ("Invalid entry sector") whenever no portal
  route reaches the target, such as any order out of a walled-in area.
- A red arrow pointing at the ground marks the rally point. Only the Hunter
  sees it, even where Builders have vision. It cannot be clicked.
- A box selection that catches horses selects only the horses (AoE2 style).
  The host also ignores the Hunter tower if it arrives in a horse order, so the
  horses still obey.
- New `SET_RALLY_POINT` command kind (wire value 9) in protocol 16.

## Capabilities

### New Capabilities

- `fvd-hunter-rally-point`: settable, Hunter-only rally point for spawned horses.

### Modified Capabilities

- `fvd-hunter-spawn-tower`: horses still spawn beside the tower; they ride to
  the rally point when one is set instead of receiving no order.
- `fvd-hunter-stances`: box selections prefer horses over the Hunter tower.
