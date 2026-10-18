# Tasks

- [x] 1.1 Evaluate Windows-compatible transport and define a versioned protocol with sessions, command framing and replication.
- [x] 1.2 Prove loopback and two-process movement command-to-authoritative-state synchronization.
- [x] 1.3 Bind each connection to its lobby-assigned Hunter/Builder identity; reject spoofed Builder orders and Hunter horse Move/AttackMove attempts.
- [x] 1.4 Replicate initial map/players, movement, building orders, tile reservations and completed towers.
- [x] 1.5 Replicate first horse at match t=20s and one per second thereafter; keep uncommanded horses idle and synchronize Hunter selection-driven AttackMove, arrow impacts, deaths and per-player kills.
- [x] 1.6 Route only the current player's kill count into their HUD; do not add a public kill leaderboard (recipient-bound score and guest HUD binding; physical multiplayer GUI verification remains under 1.8).
- [x] 1.7 Handle rejected commands and mid-game disconnects without divergent state; document policy (authoritative lifecycle coordinator, session guard, revision/broadcast tests, client host-loss terminal state; physical-machine integration remains under 1.8).
- [ ] 1.8 Verify two physical Windows PCs first, then concurrent Builder orders and human Hunter group AttackMove with up to eight human participants.
- [x] 1.9 Pair outbound host and guest connections with role-specific lobby credentials, preserve the existing authoritative session protocol through a frame relay, and verify a gameplay command across three processes.
- [x] 1.10 Admit a bounded set of provisioned relay pairs concurrently, support lobby cancellation, and isolate each pair's forwarding and disconnect lifetime.
- [x] 1.11 Provide a runnable one-match relay command, close stalled admissions at completion, and verify independent pairs through the service boundary.
