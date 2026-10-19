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

The `RelayRoomDirectory` now reserves seven independently forwarded
host channels; `RelayMultiHostLobby` joins them to one `HostLobby`,
broadcasts roster revisions, starts one role lottery and rebinds each
session. A dedicated test exercises three/eight player public rooms,
bootstraps and command round trips. The game GUI now selects the
multi-host admission manager when the host chooses a 3–8-player room.

The three/eight-player `fvd_multi_session` test exercises the current
authoritative command bridge, unique TCP `ConnectionSession` objects, per-peer
`WorldDeltaBaseline` instances and `MatchDisconnectCoordinator` without
depending on the public relay UI. This validates the multi-session core,
A separate `lobby_relay_client` test now covers the live three-client
runner's role assignment, recipient-private snapshots, command replies,
Builder departure, and surviving players' continued orders. The multi-host
GUI also uses that runner, but Windows and physical-machine GUI tests remain
outstanding.

The remaining integration must:

1. **Implemented in the relay and host admission manager:** a room stays
   joinable until its reserved channels (up to seven) are paired. Each
   channel has an independent authenticated TCP sequence stream, and
   public rooms use a separate private host-slot capability.
2. **Implemented in `RelayMultiHostLobby`:** a single host-owned
   `HostLobby` broadcasts each roster revision to all admitted guests
   and sends one immutable `START_MATCH` mapping.
3. **Implemented:** Configure authoritative capture with the admitted
   player count, mark unused slots disconnected, and remove unassigned
   starting Builders on the simulation owner thread before bootstrap.
4. **Implemented:** Capture recipient-specific bootstrap snapshots
   sequentially and await every surviving guest's `MATCH_READY`.
   Maintain one delta baseline per recipient and private scores.
5. **Implemented:** Queue intents from all authenticated peers onto the
   single simulation thread; verify each session again just before execution.
   Sequence concurrent construction events on the host and guard callbacks
   with the source identity and session generation.
6. **Implemented for guest departures:** A Builder leaving retains
   existing entities and ownership, loses command access, and generates
   `PARTICIPANT_DISCONNECTED` and a subsequent revisioned world update.
   Hunter loss distributes terminal notices and closes the room. A lost
   authoritative **Builder host** still requires 1.17.
7. Exercise the public-room runner with 3, 4 and 8 local game processes
   including at least one human Hunter, concurrent tower sites, deaths,
   stale-command rejections, guest loss, host loss and F10 rematches.
   Verify Windows and separate physical PCs before marking 1.16c complete.

Avoid replacing the working two-player code path until the new multi-guest
runner has automated coverage, to prevent regression of the playable build.

## 1.17 Hunter authority continuity

**Current limitation:** The original lobby creator still runs the only
authoritative simulation, regardless of role. Builder *guest* disconnections
work, but losing that authoritative Builder destroys the only event loop and
closes every pair forwarded through the old relay room. The guest
`ReplicationTracker` holds only presentation data and cannot resume that
simulation.

**Preferred design: move authority before gameplay rather than after a
crash.** Keep the existing weighted Hunter lottery and the lobby creator's
"host room" UI. Once `START_MATCH` chooses the Hunter:

1. If the creator is Hunter, retain the current host-authoritative path.
   If the creator is Builder, the Hunter starts a second, **Hunter-owned**
   relay room while the original lobby is still connected. All participants
   learn the new room ID and a distinct, single-use, old-player-bound
   rejoin ticket over their authenticated original session.
2. Each Builder (including the original room creator) connects to a separate
   Hunter-owned relay channel. A ticket must match the previous match
   identity and exact assigned player slot; never infer player ownership
   from display names, join order or an unauthenticated public room token.
   The rehost room's participant numbers are new; `start_fixed()` carries
   the original game-player slots instead of running a second role lottery.
3. The Hunter confirms the entire new session roster before publishing the
   final authority lease. `AuthoritySuccession` requires unique new session
   identities and fences the old authority epoch. Abort and return to the
   lobby if handoff cannot finish; don't start competing simulations.
4. All clients close their old gameplay transport (without erasing the
   immutable role draw), switch the presenter/controller to the new
   authority channel and send the ordinary `WORLD_SNAPSHOT` /
   `MATCH_READY` bootstrap. The Hunter begins the *one* authoritative
   simulation at paused time zero. Only then does the five-second reveal
   release the host clock and accept commands.
5. A Builder who originally hosted can now disconnect just like any other
   Builder: it has no simulation authority or relay channel that other peers
   depend on. Other Builders remain connected to the Hunter and retain
   their towers and units. Hunter loss ends the match.

**Implemented foundation:** The role/epoch succession ledger, checkpoint
fencing, full-rejoin preparation barrier, and role-preserving
`RelayMultiHostLobby::start_fixed()`, with unit/transport regression tests.
The original two-peer and multi-guest live game paths are unchanged
until the complete redirect/rebind transaction is present. Do **not**
advertise continuity on a Builder-host disconnect yet.

**Fallback for an actual mid-match authority change:** A complete restartable
engine checkpoint is necessary. A `WorldState` lacks the authoritative
event queue, entity component curves, outstanding command acknowledgements,
construction reservations and timers, scheduled Hunter spawns, combat
cooldowns, and any stochastic engine state. The existing
`AuthoritySuccession::attest_restored_checkpoint()` gate is a planning API,
not an engine restore function. No production caller may attest success
until a checkpoint has been fully restored and checked for parity.

## Risks

Network timing and simulation time-indexed curves could be mistakenly
mixed. Avoid trusting client timestamps or ownership claims; stamp and
order accepted commands at the host. Do not let clients switch into another
player ID or change the authoritative role mapping. Multiple simultaneous
tower orders require single-host contention resolution.
