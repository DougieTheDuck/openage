# Proposal

## Why

Without moving enemies, firing towers, death and kill attribution, FVD
cannot yet demonstrate the core defense loop or meaningful kill totals.

## What Changes

- During a running match, spawn one Hunter horse per second at map center.
- Move spawned horses towards the Builder defense area; exact routing and
  human Hunter controls remain design decisions.
- Let completed towers automatically fire arrows at Hunter units and apply
  damage until a unit dies.
- Record exactly one kill for the owner of the tower dealing lethal damage.
- Expose local totals to the HUD without displaying other players' totals.

## Capabilities

### New Capabilities

- `fvd-hunter-combat`: horse spawning, movement, tower arrow combat, death
  and owner-based kill credit.

### Modified Capabilities

None.

## Impact

- FVD entity data and simulation combat/events
- Unit positions, health/destroy lifecycle and renderer attack effects
- Player-owned kill tally and HUD binding

Player 1 is a **human Hunter in multiplayer**, NOT an AI participant.
Automated spawning and any basic horse movement must not be confused with
the player role. Solo developer mode has no AI opponent/player.
