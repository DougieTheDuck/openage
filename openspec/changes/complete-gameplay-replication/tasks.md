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
- [x] 1.3a Reduce per-entity update traffic: compare each new position
  against the previously transmitted authoritative movement curve,
  suppress an unchanged moving entity, and advance the guest's position
  locally using that same curve. Genuine corrections, health changes and
  rerouted paths still transmit. Regression simulates 512 horses across
  2/4/8-player rosters and asserts compact deltas, exact positions and
  correction transmission. Future-key capture now starts at the first
  future key rather than rescanning old curve history. **Needs builds/runs.**
- [ ] 1.3b Benchmark wall-clock CPU, bandwidth and jitter under actual
  2/4/8 clients and dense combat. Check occasional authoritative
  corrections and recovery snapshots under artificial latency.
- [ ] 1.4 Verify synchronized entity orientation and appropriate sprite
  animation changes/phase using scheduled angle/animation curves, where
  animations exist; remove any guest-only guessed behavior.
  - [x] 1.4a Capture authoritative sampled facing and bounded future
    instantaneous turn keys from the simulation's segmented angle curves.
    Carry them through snapshots, motion-suppressed deltas and guest
    RenderEntity sprite direction. Add wire order/rejection, timed expiry
    and retask correction regressions; protocol v11 rejects older peers.
    **Awaiting Windows/Linux builds and 2/4/8-client visual verification.**
  - [x] 1.4b Wire Idle/Move animation identity and authoritative start
    timestamp through host snapshots and guest deltas, including sprite
    selection and consistent loop phase after recovery. Demo asset set uses
    existing 16-angle Orc idle/walk spritesheets as a temporary Builder.
    **Awaiting compilation and on-screen Windows/Linux verification.**
  - [ ] 1.4c Validate frame-by-frame parity, proper direction mapping, 2/4/8
    process animation changes and animation/resource budget on real hardware;
    extend to Horse-specific artwork, attacks, building and death.
- [x] 1.5a Retain uniquely identified arrow launches for a bounded
  3-second post-impact history; deliver them in normal recipient deltas
  even when the 0.2-second flight ends between captures. Guests dedupe
  IDs and replay a late-received visual locally without applying damage.
  Regression covers missed-between-captures, no duplicate event, event
  expiry and over-age rejection. **Needs builds/runs.**
- [ ] 1.5b Add guaranteed no-loss handling when more than the current
  2048-event history cap fires before a recipient catches up (event
  cursor/batched acknowledgements or overflow recovery), plus genuine
  hit/death/impact FX and integration tests for latency, recovery and
  authoritative combat/render parity. Do not mark 1.5 complete until
  those and cross-client verification pass.
- [x] 1.6a Restore match-sized starting Builder pruning for both legacy
  two-peer Hunter-creator matches and multi-host matches. The lobby reports
  match_player_count=2 through the legacy assignment/bootstrap, the presenter
  configures pruning regardless of transport mode, and the simulation now
  refuses snapshots before participant admission or if the initial world
  contains the wrong number of Builders. Added the `fvd_network_builder_count`
  asset-backed 2/3/8-player regression and lobby status checks.
  **Awaiting Windows/Linux execution.**
- [ ] 1.6b Display all Builder construction phases, tower ownership,
  reservations and planned/queued foundations consistently for Hunter
  and Builders, including cancellation and retasking.
  - [x] Transmit actual phase start/end timestamps (wire protocol v9), use
    host-authored timed progress in the guest HUD, and test wire roundtrip,
    construction-only deltas and loopback playback. **Awaiting build/run.**
  - [x] Derive uniform host/guest site overlays from authoritative Towers,
    Foundations, active orders and Builder-owned reserved tiles. Show owner
    labels and queued/planned/building/complete phases, correct guest queue
    totals, and test cancellation/retasking and loopback phases.
    **Awaiting build/run and live visual acceptance.**
  - [ ] Visually verify all 2/4/8-player screens and real camera movement,
    conflicting reservations, cancellation and retasking on Windows/Linux.
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

- Guest positions can now advance on authoritative future motion curves
  without a new per-entity record each capture; cross-platform builds,
  gameplay-view parity and network recovery hardening remain unverified.
- Entity facing/animation phase and hit/death effects lack independently
  verified replication/presentation.
- Timed arrow launch events now remain available briefly after impact,
  including late guest visuals, but high-volume overflow, impact/death
  feedback and all-player visual verification remain outstanding. Attack
  orders, construction states and private scores already have wire records.
- Network state still uses one recipient-oriented world capture at a time.
  Ensure update cadence and performance do not deteriorate with players.
