# FVD prototype verification matrix

**Last audited:** 2026-10-28, branch `FVD`. **Evidence status:** tests and
wire paths reviewed, not executed here. The reviewing environment cannot
clone/build openage or run the FVD asset-backed binary. A registered test
is **not** a test pass. Do not mark hardware/visual acceptance complete
from these test names alone.

## Automated preflight — Linux and Windows

First build the same current `FVD` commit and ensure the converted FVD
assets are installed. From the repository root, run:

```sh
./run test -a --have-assets
```

The last independently reported Linux result was **82/86**, prior to
subsequent fixes. The fresh result must be recorded before marking these
items verified. On Windows, use the configured `bin/run` launcher
(e.g. `bin/run test -a --have-assets`).

If the full run fails, rerun the relevant named tests, with `-a --have-assets`
for asset-backed tests:

```sh
bin/run test openage::network::tests::world_delta
bin/run test openage::network::tests::messages
bin/run test openage::network::tests::replication
bin/run test openage::network::tests::lobby_capacity
bin/run test openage::network::tests::lobby_relay_client
bin/run test openage::network::tests::fvd_loopback
bin/run test openage::network::tests::fvd_multi_session -a --have-assets
bin/run test openage::gamestate::tests::fvd_hunter_spawning -a --have-assets
bin/run test openage::gamestate::tests::fvd_hunter_focus_attack -a --have-assets
bin/run test openage::gamestate::tests::fvd_hunter_combat -a --have-assets
bin/run test openage::gamestate::tests::fvd_scenario -a --have-assets
```

Use the installed launcher path appropriate to each machine. Repeat
`fvd_scenario` and `lobby_capacity` multiple times to expose intermittent
selection and startup failures; a single pass cannot rule out flakiness.
Run `make tests` and `make checkmerge` where that build workflow is
configured.

### Verification mapping

| OpenSpec scope | Automated evidence currently present | Status |
|---|---|---|
| Movement curves / sparse traffic (1.1–1.3a) | `world_delta` (512 horses, 2/4/8 player rosters, changed routes, packet size), `messages`, `replication` | Written; execute on both OSes |
| Facing and animation (1.4a–b) | `messages`, `world_delta`, `fvd_animation_phase` (includes same-action ATTACK restart and recovery) | Written; execute; visual parity still required |
| Projectile sequencing / late playback (1.5a–b) | `world_delta` (4,096 launches in two 2,048 batches), `messages`, `fvd_hunter_combat`, `fvd_hunter_focus_attack` (cancel after shot launch; no HP loss), `fvd_loopback` | Written; execute; live congestion unverified |
| Construction and HUD state (1.6a–b) | `fvd_network_builder_count`, `fvd_construction`, `fvd_build_queue`, `fvd_build_line_order`, `fvd_retask_foundation`, `fvd_presentation_snapshot`, `replication` | Written; execute; Qt input/visual checks remain |
| Hunter spawns, targeting and orders (1.7) | `fvd_hunter_spawning` (19.999/20/21/22 seconds), `fvd_hunter_attack_move_regressions`, `fvd_hunter_focus_attack`, `fvd_hunter_stances`, `world_delta` | Written; execute; target highlighting still needs human inspection |
| 2–8 player protocol / recovery (1.9) | `lobby_capacity`, `lobby_relay_client`, `fvd_multi_session`, `fvd_loopback`, `replication`, `world_delta`, `authority_succession` | Written; execute; real Windows/Linux processes remain |
| HUD input / cancel (build-menu 1.6) | `tower_placement_mode`, `hud_mouse_capture`, construction tests | Written; execute; mouse/QML behavior requires game |
| Tower kill credit (combat 1.9) | `fvd_hunter_combat`, `replication` | Written; execute; selected-tower kill label needs visual check |
| Endgame (1.10) | No victory/defeat system implemented yet | Not verifiable |

**Important boundaries:** `lobby_capacity` runs real relay-client workers
but not graphical game processes. The 512-entity regression tests encoded
size/parity, **not** wall-clock bandwidth, CPU or jitter. Transport tests
cannot demonstrate visual synchronization, actual camera behavior, or
Windows/Linux compatibility.

## Manual acceptance — user / collaborator

There are **five** meaningful hands-on sessions to collect; ideally use
the same commit on every machine and save game/relay logs:

1. **Solo UI and construction** — click and drag-select Builders (especially
   small boxes and different zoom/camera positions); verify Build Tower
   button, B/Cancel/Escape, Shift-drag/queued tower foundations, progress,
   cancellations and per-Tower versus total kill display.
2. **Two real game clients** — Hunter/Builder movement, 4-tile ranged
   attacks, correct horse facing and animation restart, tower and Hunter
   projectiles, confirmed HP change and death effects, construction
   ownership/labels while cameras move, private kill totals.
3. **Network adversity** — add packet delay/jitter/throughput limits or
   pause one recipient; verify backpressure recovery, no duplicate shots,
   no divergent damage, an interrupted/retasked Hunter attack, snapshot
   recovery, and explicit behavior if the 65,536 unsent-event guard fires.
4. **Four and eight graphical game processes** — exact role assignment,
   simultaneous contested tower reservations, attack/movement concurrency,
   spawn cadence and performance (host CPU, recipient bandwidth, frame
   latency). Repeat Hunter authority handoff when lobby creator becomes
   Builder; disconnect a Builder, then the Hunter.
5. **Cross-platform and repeated starts** — one Windows/Linux pair,
   repeated fresh matches and F10 reset, no stale lobbies, stuck role
   overlays or lost commands; record the Linux ten-start rehost
   reproduction result.

See [multiplayer acceptance](WINDOWS_MULTIPLAYER_ACCEPTANCE.md) and
[build/HUD interactive checks](../../../openspec/changes/build-menu-hud/verification.md)
for the step-by-step details. Physical-client tests are **not replaced**
by loopback or worker-level harnesses.

## User manual smoke-check results — 2026-10-28

These are **user-reported** gameplay observations, not automated test executions or completion of the comprehensive 2/4/8-player / cross-platform acceptance matrix. Keep broader verification items open until their full criteria are met.

| Smoke check | Result | Scope / follow-up |
|---|---|---|
| 1. Multiplayer synchronisation | **Untested** | Full multi-client state parity remains pending. |
| 2. Tower construction queues | **PASS** | User confirmed the instructed hands-on Builder FIFO Shift-drag and network queue smoke check; does not independently certify 2/4/8-client coverage or all edge cases. |
| 3. Hunter combat and stances | **FAIL** | Patrol currently interferes with stance behaviour. Track fix in [hunter-stances task 1.12](../../../openspec/changes/hunter-stances/tasks.md): stances must have the same acquisition/engagement/disengagement semantics while patrolling as while stationary; Linux developer to implement and user to retest. |
| 4. Disconnects and host migration | **PASS** | User confirmed the instructed hands-on disconnect / host-migration smoke check; leave repeated starts, multi-client and cross-platform coverage open. |
| 5. Building destruction and rendering | **PASS** | User confirmed the instructed hands-on destruction / depth / shadow smoke check; not a substitute for full graphical parity and recovery tests. |

No tests were run by this documentation update. Do not mark the original broad OpenSpec implementation/acceptance milestones complete solely on this smoke-check evidence.

## Outstanding implementation versus verification

**Still implementation:** 1.7 any missing network target-highlight/rally
behavior; 1.10 authoritative victory/defeat and result feedback; optional
legacy mid-match Builder-owned host takeover, if retained as a requirement.

**Still verification:** 1.3b measured CPU/bandwidth/jitter; 1.4c sprite
phase/parity; 1.5b live projectile volume/backpressure and damage FX;
1.6b overlays across cameras; 1.9 real-process recovery/cleanup; Windows,
Linux and multi-client confirmations. Record observed outcomes before
updating OpenSpec checkboxes.
