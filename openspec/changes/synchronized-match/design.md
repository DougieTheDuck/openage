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

Use TCP connections through native Winsock for the required Windows implementation.
TCP supplies ordered, reliable lobby, command and replication delivery without adding a dependency.
The networking subsystem owns sockets and framing and remains independent from Qt Quick and the renderer.

Keep direct guest-to-host TCP for LAN play and automated transport tests.
For Internet play, the authoritative host and every guest make outbound TCP connections to a relay so router port forwarding is not required.
Use one relay connection per host-to-guest pair, allowing the existing host and guest handshake, session identity and sequence validation to run unchanged after pairing.
The lobby supplies separate single-use host and guest bearer tokens for every pair.
The relay holds a bounded registry of provisioned pairs, admits each role independently and removes cancelled or completed admissions.
Each completed pair forwards frames independently, so a disconnect closes only that pair and never runs or changes the authoritative simulation.
Transport encryption and lobby-to-relay provisioning remain required before a public deployment.

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
Relay admission uses zero session and sequence before that handshake, and relay messages are invalid after a `ConnectionSession` is accepted.

Clients submit player-independent command intents and receive a result for each
intent.
The host stamps accepted work with authoritative simulation time and a
monotonic world revision.
Clients bootstrap from a full snapshot, apply only contiguous ordered deltas,
and request another snapshot after a revision gap.
The first version uses no client-side prediction.

## Live 3–8-player integration boundary (1.16c)

The three/eight-player `fvd_multi_session` test exercises the current
authoritative command bridge, unique TCP `ConnectionSession` objects, per-peer
`WorldDeltaBaseline` instances and `MatchDisconnectCoordinator` without
depending on the public relay UI. This validates the multi-session core,
**not** live 3–8-player public rooms. The current `RelayRoomDirectory`
consumes the waiting host socket when the first guest joins, and
`RelayLobbyClient` maintains only one game transport and one remote
recipient. Raising lobby's `max_players` alone would create false capacity.

The remaining integration must:

1. Keep an advertised room joinable until 2–8 participants are admitted
   (bounded to seven independently authenticated host/guest channels).
   Each channel must have its own TCP sequence state, and a disconnect must
   close only that channel.
2. Maintain a single host-owned `HostLobby` roster. Broadcast the updated
   `PARTICIPANT_SNAPSHOT` to all admitted guests; `START_MATCH` must include
   the exact same final roster and unique role/slot mapping.
3. Configure the authoritative connection state from the admitted roster
   before bootstrap, so unused scenario slots are disconnected. Remove or
   suppress the seven-minus-N unused scenario Builders for the finished game.
4. Capture recipient-specific bootstrap snapshots sequentially on the
   simulation thread and require each guest's `MATCH_READY` before starting
   the common host clock. For live updates, keep **one delta baseline per
   recipient** and never share another player's private kill count.
5. Process authenticated intents from all live peers on the single host
   simulation thread, validating ownership against persistent connection
   state and resolving simultaneous construction conflicts in one event
   order. Bind deferred command results to their originating peer's
   session ID and connection generation.
6. On Builder loss, mark their fixed slot disconnected, preserve entities,
   and send `PARTICIPANT_DISCONNECTED` followed by one revisioned delta
   to remaining peers. On Hunter loss, distribute the terminal notice and
   stop the match. A lost authoritative **Builder host** still requires
   separate continuity work under 1.17.
7. Exercise the public-room runner with 3, 4 and 8 local game processes
   including at least one human Hunter, concurrent tower sites, deaths,
   stale-command rejections, guest loss, host loss and F10 rematches.
   Verify Windows and separate physical PCs before marking 1.16c complete.

Avoid replacing the working two-player code path until the new multi-guest
runner has automated coverage, to prevent regression of the playable build.

## Risks

Network timing and simulation time-indexed curves could be mistakenly
mixed. Avoid trusting client timestamps or ownership claims; stamp and
order accepted commands at the host. Do not let clients switch into another
player ID or change the authoritative role mapping. Multiple simultaneous
tower orders require single-host contention resolution.
