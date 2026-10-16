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

Gameplay payloads never contain a claimed player ID.
The network boundary passes the decoded `CommandIntent` and the session's assigned player to `submit_command_intent`.
The bridge schedules work on the authoritative event loop and returns a `CommandResult` decision.
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

`WorldSnapshot` and `WorldDelta` use the same position payload.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Base world revision |
| 8 | 8 | World revision carried by this message |
| 16 | 8 | Authoritative simulation time as its raw signed fixed-point value |
| 24 | 4 | Position count, at most 32,767 |
| 28 | `32 * count` | Position records |

Each position record contains an 8-byte entity ID followed by its three signed Q16.16 coordinates.
The position-count limit keeps the complete payload within the 1 MiB frame limit.

### Snapshot request payload

`SnapshotRequest` carries the client's last applied world revision as one 8-byte unsigned integer.
It is sent once when a client receives a delta whose base does not match that revision.
The client suppresses later deltas until a complete recovery snapshot arrives.

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
