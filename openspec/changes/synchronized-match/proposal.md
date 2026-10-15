# Proposal

## Why

A lobby alone does not make FVD multiplayer: separately running simulations
must agree on units, movement, construction, combat and local kill counts.

## What Changes

- Run a host-authoritative match for 2-8 humans, including the human Hunter.
- Clients submit intents for their assigned units, including human Hunter
  multi-horse Move/AttackMove commands; the host validates and processes
  them in simulation order.
- Replicate world entities, construction occupancy/state, delayed horse
  spawns, Hunter orders, projectiles, damage, deaths and kill totals.
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
