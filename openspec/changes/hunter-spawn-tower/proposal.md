# Proposal

## Why

The Hunter's central horse spawn area needs a permanent defensive landmark that prevents Builders from occupying or camping the spawn indefinitely. It should become dangerous only when the Hunter's 20-second head start expires.

## What Changes

- Create exactly one **Hunter spawn tower** at the map center when each match starts, owned by the Hunter (player 1).
- Spawn Hunter horses in free, valid positions **around** its footprint, beginning at the existing match-time 20-second deadline and continuing at one horse per second.
- Make this tower permanent and indestructible. It cannot be damaged, demolished, sold, replaced, or removed by ordinary construction/combat commands.
- Activate its automatic attacks at **t=20 seconds**, on the same simulation clock as the first Hunter horse. Before then it does not fire.
- Once active, target living enemy **Builder units** (not Hunter horses or friendly units) within **10 map tiles**, firing missiles that each deal **100 damage** on an authoritative hit.
- Replicate the tower, missile effects, damage and deaths so host and guests see the same outcome.

## Capabilities

### New Capabilities

- `fvd-hunter-spawn-tower`: central permanent tower, safe surrounding horse spawn positions, time-gated long-range defense.

### Modified Capabilities

- `fvd-hunter-combat`: horses spawn around the central tower rather than on top of its footprint; the Hunter's existing 20-second spawn timer also gates tower attacks.
- `fvd-synchronized-match`: replicate spawn tower identity and combat events without allowing client-authored damage or demolition.

## Scope

This is an **OpenSpec addition**, not a gameplay implementation. Builder towers and their normal stats, kill attribution and construction rules remain unchanged.
