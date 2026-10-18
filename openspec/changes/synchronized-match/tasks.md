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
- [x] 1.11 Provide a runnable persistent relay command, close stalled admissions at shutdown, and verify independent rooms through the service boundary.

- [x] 1.12 Carry recipient-specific `WORLD_DELTA` frames on the public-room lobby's existing authenticated host/guest session after `MATCH_READY`; queue bounded snapshots on the host, apply decoded revisions on the guest, and exercise skipped-capture revisions in the relay regression test. The presenter now samples the paused simulation after the five-second reveal.
- [x] 1.13 After the five-second reveal, hide the lobby and resume the host-authoritative clock, forward guest Move/AttackMove/BuildTower `COMMAND_INTENT` and host `COMMAND_RESULT`, and render host `WorldState` sprites directly on the guest. Two-player integration still needs physical Windows testing.
- [x] 1.14a Add `SNAPSHOT_REQUEST` recovery and recipient-specific full re-sync from the host's last transmitted delta baseline, with public-room regression coverage.
- [ ] 1.14b Complete authenticated live-session disconnect policy and recoverable transient transport errors.

- [x] 1.15a Add host-validated guest focus-target attacks, Shift+queued tower builds, and queued-foundation prioritization on the authenticated intent wire format.
- [x] 1.15b Show recipient-authoritative queued sites, construction phase, and hunter countdown in the guest HUD; F10 resets the game/event/entity state, resets the paused clock to zero and reattaches cleared world/terrain render stages for clean rematches. Physical Windows rematch verification is outstanding.
- [ ] 1.16 Finish live-session disconnect policy, command-result session-generation guards, special-order HUD polish, and full integration tests on Windows.
