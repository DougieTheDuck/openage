# FVD playable multiplayer roadmap

## Target experience

A host and at least one remote friend (for a total of 8 players max) can join an FVD lobby and start a shared match. 
Player 1 is always the Hunter and doesn't get to build structures. 
Player numbers are assigned at random to players from the lobby when the game is 
started but the lobby host has 50 percent less chance to get the Hunter role.

### Gameplay
Each builder can select and move their own Builder, choose
a tower from the UI, place an order to build it on any free tile, and watch the 
Builder walk to an adjacent tile and construct it. Towers defend against approaching 
Hunter units. All players see compatible game state and scores but only see their kill count,
which is incremented for every hunter unit one of their towers kills.

Solo FVD remains available as a development/test mode where a hotkey switches 
control between player 1 and player 2 (there is no ai).

## Observed baseline (FVD branch, 2026-10-15)

- A 128×128 grass map and eight player IDs are created from FVD scenario data.
- Players 2–8 each receive one Builder; player 1 receives no Hunter units yet.
- The local controller starts as player 2, can select and move Builders, and
  uses keys 1–8 to switch simulated ownership for debugging.
- Ctrl-left-click currently spawns a tower instantly after owner and tile
  validation. There is no construction order or building timer.
- The FVD QML UI is an instructional banner, not a usable build menu.
- No FVD combat/kill tracking or network/lobby implementation was found.
- Movement and pathfinding, game commands, the event queue, the QML renderer,
  and simulation tests provide an existing foundation.

See assets/fvd/README.md, libopenage/gamestate/fvd.cpp and
libopenage/gamestate/fvd_test.cpp for the current behavior.

## Deliverables in dependency order

### 1. Builder construction — specified

Change: [builder-construction](changes/builder-construction/proposal.md)

- A Builder receives an explicit build command for a chosen vacant tile.
- The Builder finds a reachable adjacent stand position and walks there.
- Construction takes a finite configurable time; the tower only becomes
  functional on completion.
- Tile reservation, conflicting requests, canceled orders, unreachable
  positions, and ownership checks are handled in the simulation.
- Existing Ctrl-click can exercise the order until the build UI is available.

**Done when:** a local player can order a distant tower, watch the Builder
approach and complete it; interruptions/invalid requests leave no ghost tower;
relevant regression tests pass.

### 2. Build menu and gameplay HUD — next proposal

- Display the controlled player's identity and selected unit.
- With an owned Builder selected, show a Build Tower button with a placement
  preview, clear valid/invalid location feedback and Escape/right-click cancel.
- Expose construction state/progress where practical.
- Reserve a visible field for the local player's kill count, initially zero.
- Make UI actions produce the same simulation commands as keyboard shortcuts.

**Done when:** placing a tower needs no developer shortcut and mouse events
over the menu do not unintentionally select or command the world.

### 3. Hunter pressure, tower combat, and kill credit — next proposal

- Player 1 AI spawns and steers basic Hunter attackers from the center of the map.
- One horse unit is spawned per second
- Completed towers autonomously target enemies, fire arrows, inflict damage and kill.
- Credited kills belong to the **owner of the tower that dealt lethal damage**;
  no double-credit, including near-simultaneous attacks.
- Show the current human player's kill total in the HUD; totals persist for
  the match. Test killing, attribution and cleanup in the simulation.

**Done when:** Hunter units reach the build area, towers stop them, and kill
totals advance only for the correct Builder. A full victory/economy system is
not a prerequisite for the first multiplayer build.

### 4. Host/join lobby — next proposal

- Host a session, join via address/port, display participant names and
  unique Builder slots, and let participants mark ready.
- Start only when the host chooses Start
- Assign only one human per Builder slot; reject incompatible clients, full
  games and attempts to claim the Hunter slot.
- Starting a match selects a random player to be hunter (player 1) with less chance of the host being hunter.
- First transport target: direct IP/LAN; account services, NAT traversal,
  public matchmaking, spectators and mid-match joining are out of scope.
- Cross platform network implementation is a bonus, but windows only is required.

**Done when:** two machines can connect, agree on slots and start together.

### 5. Shared live match — next proposal

- Prefer **host-authoritative simulation** for the first version. Clients send
  input intents; the host validates player and entity ownership and runs the
  game rules. The host transmits ordered world-state changes or snapshots.
- Synchronize movement, construction and occupancy, enemy activity, tower
  attacks, deaths and per-player kills. Clients cannot choose another owner's
  identity merely by forging a command.
- Clearly handle disconnects and rejected input; no silent divergence.
- Test with two physical machines first, then 3–7 human participants.
- Keep the renderer independent of networking and use explicit simulation
  state transfer rather than sending mouse/UI events.

**Done when:** every connected participant sees the same authoritative towers,
units, deaths and kill counts under concurrent build orders.

## Early technical risk to investigate

Networking is the largest unknown: the FVD branch has simulation and event
infrastructure but no dedicated match transport, lobby, replication or session
model. Before committing to a network library or protocol, prove a loopback
host/client connection and that simulation events can be applied safely and
observed by two independently running processes. Document transport choice,
state replication granularity, time synchronization, reconnection policy and
version compatibility in the networking changes.

## Scope boundaries for this playable target

Keep one map, one buildable tower, the current single Builder per player,
simple Hunter attacks, no resource economy and no competitive ranking.
Gameplay should be correct and visibly responsive before animation polish.
No server directory, cross-network NAT solution, host migration, spectator
system or online account dependency is required.

## Project verification

- Unit/regression tests for command ownership, reserved cells, cancellation,
  path arrival, completion, combat attribution and stable kill counting.
- Interactive GUI check: selecting, hovering, placing, canceling and building.
- Multiplayer integration check on two separate processes/machines and then
  with more players: conflicting orders, disconnect, concurrent attacks,
  state consistency and restart/exit.
- Follow repository instructions: make tests and report any
  checks blocked by development dependencies or runtime assets.
- This project is a separate game fork, so don't worry about upstream compatibility or merges