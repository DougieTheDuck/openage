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

Host listens at an explicitly selected address/port; Windows direct-IP/LAN
is enough. Joiners identify themselves and are accepted only if the game
is compatible and capacity remains. List participants in the lobby.
The host alone decides when to start; there is NO required ready-vote gate
in the revised roadmap. At least two humans must be present at start.
Broadcast assignment and game-start parameters once, consistently.

Only connected lobby participants get a controlled role or initial unit.
The scenario's existing fixed eight-player local test setup must be
separated from network session creation.

## Open Decisions

- Transport/protocol, version/asset handshake and how to choose port.
- What happens if someone disconnects before Start or during role assignment.
- Name rules and whether empty lobby slots display placeholders.

Do not bake transport selection into the role lottery. No upstream
compatibility work is required by this standalone fork.
