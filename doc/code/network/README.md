# FVD networking

FVD multiplayer uses one host-authoritative simulation and direct TCP connections from each guest to the host.
The host participant submits commands through the same validation boundary but does not need a loopback socket.

See [the engine architecture](../architecture.md) for the subsystem boundaries and [the synchronized-match design](../../../openspec/changes/synchronized-match/design.md) for the gameplay authority model.

## Transport

The first transport uses TCP through Winsock on Windows.
TCP matches the low-volume lobby, command and state-replication traffic and provides the required ordered reliable stream without another dependency.
The socket implementation belongs in `libopenage/network/` and must not depend on the presenter, renderer or Qt Quick.
A BSD-socket implementation can provide the same interface later without changing the wire protocol.

`network::TcpListener` binds a host endpoint and accepts `network::TcpConnection` objects.
Connections send and receive complete `Frame` values while handling partial TCP reads and writes internally.
An orderly peer shutdown returns no frame; socket failures and truncated frames raise an `Error`.

Each accepted guest has one connection and one host-generated, nonzero 64-bit session ID.
The host binds that connection to the lobby participant and assigned player; messages never carry a client-selected player identity.
The session ID detects stale messages and routing mistakes but is not a security credential.
The direct-IP prototype does not encrypt traffic or authenticate the host.

## Session and gameplay boundary

`network::ConnectionSession` is created after a successful handshake with a host-assigned nonzero session ID and player ID.
The binding is immutable for the connection lifetime.
The connection thread owns the session object and uses it to validate every inbound session ID, negotiated protocol version and contiguous sequence number.
The same object creates outbound frames with the negotiated version, correct session ID and next sequence number.
After validation and payload decoding, connection threads place authenticated commands into a bounded `CommandMailbox`.
The simulation thread drains this mailbox in FIFO arrival order and supplies its authoritative time when scheduling commands.
A full mailbox is explicit backpressure: the connection handler must reject the command or disconnect the peer rather than dropping it silently.
Simulation output is placed into one bounded `OutboundMailbox` per accepted connection.
Its values are typed command results, snapshots, deltas or disconnect notifications; only the owning connection thread converts them to sequenced frames and writes the socket.
An outbound mailbox that remains full identifies a client that cannot keep up, so the host disconnects that client instead of silently dropping authoritative state.
Connection threads publish closed sessions through `ConnectionEventMailbox` for authoritative processing.
`MatchDisconnectCoordinator` binds each remote session to its own
`OutboundMailbox` and processes those closures on the simulation thread
**before** draining the next batch of authenticated commands. It checks outbound
capacity for both messages before changing any authoritative connection state.
For each unique closure, it permanently invalidates the session, advances
the world revision once, queues one `ParticipantDisconnected` notification to
each surviving remote, then queues a complete `WorldDelta` with that
participant's `connected = false` flag. Identical repeat close events do not
advance the revision twice. An unknown or mismatched session closure, or
insufficient outbound capacity, is fatal to the host match rather than silently
dropping replication. Outbound mailboxes remain owned by connection handlers
and must outlive their registration with the coordinator.

Gameplay payloads never contain a claimed player ID.
The network boundary passes the `AuthenticatedCommand` drained from the mailbox to `submit_authenticated_command`.
The bridge schedules work on the authoritative event loop and returns a `CommandResult` decision.
When draining commands from `CommandMailbox`, use the overload of
`submit_authenticated_command` taking `MatchConnectionState`:
it re-checks the **current** session binding before scheduling the event.
Previously accepted commands already submitted to the simulation are not
rolled back, but commands still in the inbound mailbox when a disconnect is
applied are rejected with a negative callback and do not mutate gameplay.
The connection handler reports rejection with the original command ID and
the current authoritative revision; it must never optimistically apply
the rejected command.
Gameplay code can add command behavior and replicated records without reading sockets, framing bytes or maintaining connection counters.
Network code can change connection management without changing horse, projectile, damage or building simulation.

## Frame format

TCP is a byte stream, so receivers use `network::FrameParser` to handle partial and coalesced reads.
Every integer uses network byte order.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Magic `FVDN` |
| 4 | 2 | Protocol major |
| 6 | 2 | Protocol minor |
| 8 | 2 | Message type |
| 10 | 2 | Flags, currently zero |
| 12 | 4 | Payload size |
| 16 | 8 | Session ID |
| 24 | 8 | Per-direction sequence number |
| 32 | variable | Message payload |

Payloads are limited to 1 MiB.
The handshake uses session and sequence zero.
After acceptance, each direction starts at sequence one and the receiver rejects duplicates, regressions and a session ID that does not match the connection.

The current messages are `ClientHello`, `ServerAccept`, `ServerReject`, `ParticipantSnapshot`, `StartMatch`, `CommandIntent`, `CommandResult`, `WorldSnapshot`, `WorldDelta`, `ParticipantDisconnected`, `Ping`, `Pong` and `SnapshotRequest`.
Payload schemas use fixed-width network-order integers, length-prefixed UTF-8 strings and length-prefixed arrays.

### Command intent payload

The connection supplies player identity, so the payload cannot claim an owner.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Client-generated command ID |
| 8 | 2 | Command kind: move `1`, attack-move `2` or build-tower `3` |
| 10 | 2 | Flags, currently zero |
| 12 | 4 | Entity count, at most 1,024 |
| 16 | `8 * count` | Entity IDs |
| `16 + 8 * count` | 8 | Target northeast coordinate |
| `24 + 8 * count` | 8 | Target southeast coordinate |
| `32 + 8 * count` | 8 | Target height coordinate |

Coordinates are signed Q16.16 fixed-point values serialized from their raw 64-bit representation.

### Command result payload

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Client-generated command ID |
| 8 | 2 | Status: accepted `1` or rejected `2` |
| 10 | 2 | Flags, currently zero |
| 12 | 8 | Authoritative world revision |
| 20 | 8 | Authoritative simulation time as its raw signed fixed-point value |
| 28 | 4 | Rejection-reason byte length, at most 4,096 |
| 32 | variable | UTF-8 rejection reason |

### World state payload

`WorldSnapshot` and `WorldDelta` carry complete authoritative FVD state at one simulation time.
A client replaces its replicated world after accepting either message, while the base and world revisions still enforce ordered application.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Base world revision |
| 8 | 8 | World revision carried by this message |
| 16 | 8 | Authoritative simulation time as its raw signed fixed-point value |
| 24 | 4 | Square map size in tiles |
| 28 | 8 | Recipient player ID |
| 36 | 8 | Recipient player's kill count |
| 44 | 4 | Playable player count, at most 8 |
| 48 | 4 | Entity count, at most 16,384 |
| 52 | 4 | Reserved tile count, at most 16,384 |
| 56 | 4 | Active construction count, at most 1,024 |
| 60 | 4 | Active Hunter attack-order count, at most 16,384 |
| 64 | 4 | In-flight arrow count, at most 2,048 |
| 68 | variable | Player, entity, reservation, construction, attack-order and arrow records in that order |

A player record contains an 8-byte player ID, a 2-byte Hunter or Builder role and 2 flag bytes.
Player flag bit 0 is set while that participant remains connected to the running match.
An entity record contains an 8-byte entity ID, 8-byte owner ID, a length-prefixed nyan definition of at most 256 UTF-8 bytes, three signed Q16.16 position coordinates, 2 flag bytes, 2 reserved zero bytes and signed 8-byte current and maximum health values.
Health flag bit 0 is set when both health values apply; entities without a `Live` ability carry zero values and clear the flag.
A reservation record contains two signed 8-byte tile coordinates and the 8-byte ID of the Builder holding the tile.
A construction record contains its 8-byte order, owner and Builder IDs, two signed 8-byte target tile coordinates, a 2-byte planned or under-construction phase, 2 zero flag bytes, an 8-byte phase deadline and the 8-byte foundation or tower entity ID.
An attack-order record contains an 8-byte horse ID, three signed Q16.16 destination coordinates, 2 flag bytes, 2 reserved zero bytes and an 8-byte target entity ID.
Attack-order flag bits 0, 1 and 2 indicate a target ID, arrival at the requested destination and an explicit focus target respectively.
An arrow record contains 8-byte arrow, owner and target IDs, three signed Q16.16 launch coordinates, three signed Q16.16 destination coordinates and signed 8-byte launch and impact times.
Completed towers remain in the entity list and have no active construction record or reserved tiles.
Killed entities disappear from the next complete state, while arrows disappear at their impact time and the resulting health or death is present in that same state.
The host builds state separately for each connection, so the payload contains only its recipient's kill total and cannot expose a public leaderboard.
On a guest, construct `ReplicationTracker` with the immutable player ID from the accepted session.
It rejects snapshots or deltas addressed to another player and publishes only the local count from accepted, contiguous authoritative updates.
Rejected or gapped updates leave the displayed score unchanged; host loss makes it unavailable.
Bind this tracker with `Presenter::set_fvd_replication` before the presenter starts to restrict role switching and use the replicated score in the HUD instead of the local simulation's count.
Before the initial snapshot or after host loss, the HUD shows an unavailable score rather than a guessed zero.
Protocol major version 2 introduced the complete state layout because it is incompatible with the earlier position-only payload.
Protocol major version 3 added persistent participant connection status to player records.
The encoder rejects any complete payload larger than the 1 MiB frame limit.

### Snapshot request payload

`SnapshotRequest` carries the client's last applied world revision as one 8-byte unsigned integer.
It is sent once when a client receives a delta whose base does not match that revision.
The client suppresses later deltas until a complete recovery snapshot arrives.

### Participant disconnected payload

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Nonzero player ID |
| 8 | 2 | Reason: connection lost `1`, left match `2`, protocol error `3` or host shutdown `4` |
| 10 | 2 | Flags, currently zero |

## Compatibility handshake

`ClientHello` supplies the participant name, protocol version, game build version and an FVD asset manifest digest.
The host answers with `ServerAccept` and the assigned session ID or `ServerReject` and a displayable reason.
Protocol major versions and the required asset digest must match.
A newer minor version may connect only when its advertised minimum compatible minor includes the peer's version.
The host selects the highest minor version supported by both peers and writes it into `ServerAccept` and all subsequent frames.
The accepted payload supplies both the generated session ID and the lobby-assigned player ID, allowing each side to construct the same immutable `ConnectionSession`.
Unknown message types or flags are protocol errors after acceptance.

### Client hello payload

| Field | Encoding |
|---|---|
| Minimum compatible protocol minor | 2-byte unsigned integer |
| Flags | 2-byte zero value |
| Participant name | 4-byte length and at most 128 UTF-8 bytes |
| Game build version | 4-byte length and at most 128 UTF-8 bytes |
| Gameplay asset manifest digest | 32-byte SHA-256 digest |

The build version is diagnostic information; protocol compatibility and the gameplay asset digest determine whether the host accepts the client.

### Server accept payload

| Field | Encoding |
|---|---|
| Session ID | 8-byte nonzero unsigned integer |
| Assigned player ID | 8-byte nonzero unsigned integer |

### Server reject payload

The rejection payload is a 4-byte length followed by at most 4,096 UTF-8 bytes suitable for display to the connecting player.

The asset digest covers the selected modpack manifests and gameplay data, not generated build paths or graphics that do not affect simulation.

## Commands and replication

Clients send `CommandIntent` messages with a client-generated command ID, command kind, referenced entity IDs and command arguments.
The first command kinds are Builder move/build and Hunter horse move/attack-move.
The command carries no player ID or client simulation timestamp.
The host uses the connection's assigned player, checks entity ownership and ability, and stamps accepted work with authoritative simulation time and a monotonic world revision.

The host returns `CommandResult` for every intent so rejected input remains visible to the client.
Accepted mutations appear in ordered `WorldDelta` messages.
`WorldSnapshot` contains the initial state and is also the recovery boundary if a client misses a revision.
Both replication messages carry the authoritative simulation time, world revision and the base revision needed to apply a delta.
Clients apply only contiguous revisions and request a snapshot after a gap.
`network::ReplicationTracker` owns this client-side revision state and prevents repeated recovery requests while a snapshot is in flight.

The initial implementation has no client prediction.
Clients render authoritative state and may interpolate presentation positions without changing simulation state.

## Disconnect policy

Before match start, a disconnected participant is removed from the lobby and their player slot may be assigned again.
After match start, the host keeps the disconnected player's entities, ownership and already accepted orders in authoritative state.
The player ID is not reassigned, no new commands are accepted for the closed session, and the match continues for the remaining participants.
Every remaining connection receives one `ParticipantDisconnected` notification.
The host also marks the participant disconnected in the next authoritative world revision, so a recovery snapshot cannot lose this status.
`network::MatchConnectionState` owns the fixed participant-to-session bindings and rejects a second binding after a participant disconnects.
If the authoritative host disconnects, clients end the match because no peer may silently replace its simulation.
On TCP EOF or a fatal socket/protocol failure from the host, the client calls
`ReplicationTracker::host_disconnected()`, stops issuing gameplay commands,
and displays a match-ended/disconnected state. The tracker will then reject
snapshots, deltas and recovery requests; restarting a match requires a new
connection, fresh session and a new tracker. TCP EOF is not evidence of a
successful host migration.
Failure to enqueue a connection lifecycle event is fatal to the host session because continuing would conceal a disconnect from authoritative state.
