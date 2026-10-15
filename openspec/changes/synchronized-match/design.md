# Design

## Proposed authority model

The lobby host runs the only authoritative simulation, independent of
whether that human was assigned Hunter or Builder. Each client gets a
server-bound connection/session identity and a fixed player ID from the
lobby. Send gameplay intents (selected Builder, BuildTower position, Move,
and any later Hunter command), not GUI events or client-created entities.
Server-side handlers verify that intent's sender owns the referenced units
and may use the requested ability.

Run accepted actions on the host's event loop. Transmit deterministic,
ordered world updates or snapshots with sequence/time identifiers;
clients render received state and optionally interpolate positions.
The first version should prioritize correctness over sophisticated
client-side prediction or lockstep. Keep the authoritative kill count
per player on the host and transmit only the needed local display total
for the HUD (other players must not appear in the HUD).

## Investigation before implementation

- Choose a Windows-compatible transport and packet framing/serialization.
- Prove loopback command -> host simulation -> remote observable state on
  two distinct processes, before integrating all combat and HUD systems.
- Define state bootstrap, asset/version compatibility, ordering/reliability,
  input rate limits, replay protection and disconnect handling.
- Distinguish disconnect before and after game start; decide whether
  abandoned Builder entities persist or matches pause/end.

## Risks

Network timing and simulation time-indexed curves could be mistakenly
mixed. Avoid trusting client timestamps or ownership claims; stamp and
order accepted commands at the host. Do not let clients switch into another
player ID or change the authoritative role mapping. Multiple simultaneous
tower orders require single-host contention resolution.
