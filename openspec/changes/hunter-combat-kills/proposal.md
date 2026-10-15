# Proposal

## Why

Without moving enemies, firing towers, death and kill attribution, FVD
cannot yet demonstrate the core defense loop or meaningful kill totals.

## What Changes

- Give Builders 20 seconds from match start before the first Hunter horse spawns
  at map center, then spawn one additional horse per second.
- Leave spawned horses idle at the center until the human Hunter selects one
  or more and issues an order.
- Support human Hunter Move and AttackMove orders; horses on AttackMove engage
  eligible opposing units met along their route and then resume advancing.
- Defer automatic rally/gather points and automatic movement after spawning.
- Let completed towers automatically fire arrows at Hunter units and apply
  damage until a unit dies.
- Record exactly one kill for the owner of the tower dealing lethal damage.
- Expose local totals to the HUD without displaying other players' totals.

## Capabilities

### New Capabilities

- `fvd-hunter-combat`: delayed Hunter horse spawning, human selection,
  movement/AttackMove, tower arrow combat, death and owner-based kill credit.

### Modified Capabilities

None.

## Impact

- FVD entity data and simulation combat/events
- Unit positions, health/destroy lifecycle and renderer attack effects
- Player-owned kill tally and HUD binding

Player 1 is a **human Hunter in multiplayer**, NOT an AI participant.
Only horse spawning is automatic; the human Hunter controls when and where
horses move. Solo developer mode has no AI opponent/player.
