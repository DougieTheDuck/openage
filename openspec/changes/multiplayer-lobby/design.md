# Design

## Player allocation

When the host starts a lobby containing N humans (2 <= N <= 8), perform
a single authoritative weighted random Hunter draw:

- Host weight: 0.5.
- Every other connected player weight: 1.
- Choose one participant, assign Hunter player ID 1.
- Randomly assign distinct Builder IDs 2..N to the others.

Thus the host's Hunter probability is 0.5/(N-0.5) = 1/(2N-1), and each
guest's is 1/(N-0.5). For a two-person lobby the odds are 1/3 host and 2/3
guest; for eight people they are 1/15 host and 2/15 for each guest.
This makes the host **half as likely as each individual guest**, not
necessarily 50% less likely than the aggregate guest population.

This is matchmaking state, not a player-selectable civilization switch.
The host is also a participant; hosting does not imply owning slot 1 or 2.
The host can become Hunter, in which case it still runs the server.

## Lobby flow

All players connect outbound to an explicitly selected relay address and port.
The relay stays running across matches and owns a bounded directory of waiting rooms.
A host supplies a room name and player name, receives a relay-generated room ID and remains connected while the room is advertised.
A guest refreshes the directory and joins a listed room by ID.
The first implementation pairs one host with one guest; extending a room to the required eight participants remains separate work.
Joiners identify themselves and are accepted only if the game is compatible and capacity remains.
List participants in the lobby.
The host alone decides when to start; there is NO required ready-vote gate
in the revised roadmap. At least two humans must be present at start.
Broadcast assignment and game-start parameters once, consistently.

The relay has two access modes.
With no configured player tokens, room creation, listing and joining are public to anyone who knows the relay endpoint.
With one or more configured tokens, every directory request must present a listed 256-bit bearer token.
The host and guest of a private room must use different tokens.
Tokens are reusable alpha identities rather than per-room credentials, while room IDs are discovery identifiers rather than secrets.
The relay bounds pending rooms and concurrent connections in both modes.

The authoritative game host remains a player process.
After pairing, the relay only forwards complete protocol frames and does not run the simulation or assign gameplay roles.

Only connected lobby participants get a controlled role or initial unit.
The scenario's existing fixed eight-player local test setup must be
separated from network session creation.

## Open Decisions

- How a production deployment discovers the relay endpoint and protects bearer tokens in transit.
- How room discovery and forwarding extend from the current two-player path to eight participants.
- What happens if someone disconnects before Start or during role assignment.
- Name rules and whether empty lobby slots display placeholders.

Do not bake transport selection into the role lottery. No upstream
compatibility work is required by this standalone fork.
