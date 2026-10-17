# Design

## Proposed authority model

The lobby host runs the only authoritative simulation, independent of
whether that human was assigned Hunter or Builder. Each client gets a
server-bound connection/session identity and a fixed player ID from the
lobby. Send gameplay intents (Builder BuildTower/Move and Hunter-owned horse
Move/AttackMove with selected entity IDs and target destination), not
GUI events or client-created entities.
Server-side handlers verify that intent's sender owns the referenced units
and may use the requested ability. New Hunter horses must remain idle
at the center until the assigned human Hunter orders them; spawning
alone never causes autonomous movement.

Run accepted actions on the host's event loop. Transmit deterministic,
ordered world updates or snapshots with sequence/time identifiers;
clients render received state and optionally interpolate positions.
The first version should prioritize correctness over sophisticated
client-side prediction or lockstep. Keep the authoritative kill count
per player on the host and transmit only the needed local display total
for the HUD (other players must not appear in the HUD). Spawn cadence is
relative to the host-authoritative match start: first horse at 20s, then
one per second. Clients must not schedule independent horse spawns.

## Investigation before implementation

- Prove loopback command -> host simulation -> remote observable state on
  two distinct processes, before integrating all combat and HUD systems.
- Define input rate limits.
- Follow the pre-match and mid-match disconnect policy in
  [the networking documentation](../../../doc/code/network/README.md#disconnect-policy).

## Transport and protocol

Use direct TCP connections through native Winsock for the required Windows
implementation.
TCP supplies ordered, reliable lobby, command and replication delivery without
adding a dependency.
The networking subsystem owns sockets and framing and remains independent from
Qt Quick and the renderer.

Frames use a fixed 32-byte network-order header containing `FVDN` magic,
protocol major/minor, message type, flags, payload length, session ID and
per-direction sequence number.
Payloads are capped at 1 MiB.
See [the networking documentation](../../../doc/code/network/README.md) for the
wire layout and message set.

The host assigns a random nonzero session ID when accepting a compatible
`ClientHello` and binds the connection to its lobby participant and later
player assignment.
Protocol major and gameplay asset manifests must match; minor compatibility is
explicitly advertised.
Sequence numbers reject repeated or regressing messages after the handshake.

Clients submit player-independent command intents and receive a result for each
intent.
The host stamps accepted work with authoritative simulation time and a
monotonic world revision.
Clients bootstrap from a full snapshot, apply only contiguous ordered deltas,
and request another snapshot after a revision gap.
The first version uses no client-side prediction.

## Risks

Network timing and simulation time-indexed curves could be mistakenly
mixed. Avoid trusting client timestamps or ownership claims; stamp and
order accepted commands at the host. Do not let clients switch into another
player ID or change the authoritative role mapping. Multiple simultaneous
tower orders require single-host contention resolution.
