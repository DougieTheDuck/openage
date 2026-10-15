# Proposal

## Why

The current local test mode provides all eight faction IDs to a single
controller. FVD needs a lobby that assigns those roles to distinct humans
without permitting a player to choose another person's role in-game.

## What Changes

- Allow a host and guests to join a direct-IP/LAN lobby, initially on Windows.
- Support 2-8 humans INCLUDING host, one participant per player slot.
- When the host presses Start, randomly choose a human Hunter, weighted so
  the host has half the selection weight of any single guest.
- Assign selected Hunter to slot 1 and remaining humans to unique Builder
  slots 2-8. Prevent local role switching after the match starts.
- Reject overflow and incompatible clients, and show who is connected.
- Keep solo mode separately: hotkey-switch Hunter/Builder 1/2, no AI.

## Capabilities

### New Capabilities

- `fvd-lobby-roles`: host/join, weighted role assignment, unique slots and
  host-controlled match start.

### Modified Capabilities

None.

## Impact

- Session lifecycle and FVD player initialization
- Network transport for connecting clients and agreeing on assignments
- Lobby QML interface and removal of all-player debugging controls online

Public matchmaking, NAT traversal, spectator support, host migration and
cross-platform compatibility are excluded from the required first release.
