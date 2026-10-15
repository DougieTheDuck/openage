# Proposal

## Why

A lobby alone does not make FVD multiplayer: separately running simulations
must agree on units, movement, construction, combat and local kill counts.

## What Changes

- Run a host-authoritative match for 2-8 humans, including the human Hunter.
- Clients submit intents for their assigned units; the host authenticates,
  validates and processes those intents in simulation order.
- Replicate world entities, construction occupancy/state, projectiles,
  damage, deaths and per-player kill totals to connected clients.
- Handle rejected commands and disconnections without silent divergence.
- Keep networking outside the renderer and GUI, with Windows as the
  required initial platform.

## Capabilities

### New Capabilities

- `fvd-synchronized-match`: shared authoritative state and safe concurrent
  actions across eight possible human roles.

### Modified Capabilities

None.

## Impact

- Simulation session, command transport, serialization/state replication
- Local controller role checks and presentation of replicated state
- Integration testing with multiple processes/Windows machines

NAT traversal, online directory services, host migration and spectators
remain out of scope.
