# FVD networking

FVD multiplayer uses one host-authoritative simulation and direct TCP connections from each guest to the host.
The host participant submits commands through the same validation boundary but does not need a loopback socket.

See [the engine architecture](../architecture.md) for the subsystem boundaries and [the synchronized-match design](../../../openspec/changes/synchronized-match/design.md) for the gameplay authority model.

## Transport

The first transport uses TCP through Winsock on Windows.
TCP matches the low-volume lobby, command and state-replication traffic and provides the required ordered reliable stream without another dependency.
The socket implementation belongs in `libopenage/network/` and must not depend on the presenter, renderer or Qt Quick.
A BSD-socket implementation can provide the same interface later without changing the wire protocol.

Each accepted guest has one connection and one host-generated, nonzero 64-bit session ID.
The host binds that connection to the lobby participant and assigned player; messages never carry a client-selected player identity.
The session ID detects stale messages and routing mistakes but is not a security credential.
The direct-IP prototype does not encrypt traffic or authenticate the host.

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

The current messages are `ClientHello`, `ServerAccept`, `ServerReject`, `ParticipantSnapshot`, `StartMatch`, `CommandIntent`, `CommandResult`, `WorldSnapshot`, `WorldDelta`, `ParticipantDisconnected`, `Ping` and `Pong`.
Payload schemas use fixed-width network-order integers, length-prefixed UTF-8 strings and length-prefixed arrays.

## Compatibility handshake

`ClientHello` supplies the participant name, protocol version, game build version and an FVD asset manifest digest.
The host answers with `ServerAccept` and the assigned session ID or `ServerReject` and a displayable reason.
Protocol major versions and the required asset digest must match.
A newer minor version may connect only when its advertised minimum compatible minor includes the peer's version.
Unknown message types or flags are protocol errors after acceptance.

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

The initial implementation has no client prediction.
Clients render authoritative state and may interpolate presentation positions without changing simulation state.
