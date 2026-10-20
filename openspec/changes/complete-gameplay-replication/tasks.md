# Complete gameplay replication checklist

- [x] 1.0 Audit current wire state and distinguish global world state from
  recipient-private scores; note that constructions, reservations, orders,
  entity health, deaths and timed arrows already use snapshots/deltas.
- [x] 1.1 Serialize bounded, time-ordered future position keys from the
  authoritative Position curve, deliver through full snapshots and deltas,
  and use them for guest render playback. Bump protocol major to reject
  incompatible older builds. **Awaiting cross-platform execution.**
- [x] 1.2 Add wire roundtrip, rejection, changed-curve delta and
  timed-key expiration regressions. **Awaiting compilation and execution.**
- [ ] 1.3 Reduce per-entity update traffic: with an unchanged trajectory,
  avoid resending samples that can be reconstructed from the installed
  curve; preserve periodic correction/recovery for routing and drift.
  Benchmark at 2/4/8 players and hundreds of horses.
- [ ] 1.4 Verify synchronized entity orientation and appropriate sprite
  animation changes/phase using scheduled angle/animation curves, where
  animations exist; remove any guest-only guessed behavior.
- [ ] 1.5 Fully replicate visual combat feedback (projectile trails,
  impacts, unit hit/death effects) and test render parity. Timed arrow
  launch paths are already present in world state and guest HUD. Add
  uniquely identified short-lived effect event history/delivery so shots
  occurring wholly between two recipient captures are not missed; verify
  timing, jitter, recovery and duplicate suppression.
- [ ] 1.6 Display all Builder construction phases, tower ownership,
  reservations and planned/queued foundations consistently for Hunter
  and Builders, including cancellation and retasking.
- [ ] 1.7 Replicate Hunter spawn timing, any future rally behavior, active
  orders and target highlights via authoritative time-indexed state.
- [ ] 1.8 Every new gameplay feature must add its wire/presentation
  representation and unit, loopback, multi-client and visual acceptance
  checks in the same task—not as later network polish.
- [ ] 1.9 Validate complete replay/recovery and reconnection cleanup
  across Windows/Linux and 2/4/8 real processes; do not treat transport
  tests alone as visual acceptance.
- [ ] 1.10 Add match end-state and victory feedback to the same replicated
  state/event pipeline when win/loss rules are implemented.

## Remaining gaps from the current audit

- Moving guest proxies currently interpolate position samples; new motion
  keys have been added but still need traffic optimization, real-process
  verification and interpolation/recovery hardening.
- Entity facing/animation phase and hit/death effects lack independently
  verified replication/presentation.
- Timed arrow launch/impact visuals, attack-order state, construction
  state and private scores are already represented by the networking
  pipeline, but all-player visual verification remains outstanding.
- Network state still uses one recipient-oriented world capture at a time.
  Ensure update cadence and performance do not deteriorate with players.
