# FVD prototype assets

This directory contains original development graphics for the FVD tower defense prototype.
The initial set uses a fixed orthographic isometric view, neutral upper-left lighting, transparent sprite backgrounds, and a flat seamless terrain texture.

**Animation prototype:** Builders temporarily use the existing Orc Unarmed directional
spritesheets (16 angles each) for 16-frame idle and 20-frame walk cycles. The
original `builder.sprite`, `builder.texture`, and `builder.png` are retained;
you can restore them in `data/entities.nyan`. The proof-of-concept uses the
existing images without modifying them or requiring an external graphics tool.
FVD uses the host-selected Idle/Move animation and simulation-time animation
start for both local and guest rendering, not guest-estimated velocity.

The runtime files follow the openage [texture](../../doc/media/openage/texture_format_spec.md), [sprite](../../doc/media/openage/sprite_format_spec.md), and [terrain](../../doc/media/openage/terrain_format_spec.md) formats.
The source images were generated with OpenAI's built-in image generation tool and resized for an HD gameplay presentation.
The prompts used for the initial set are recorded in [prompts.md](prompts.md).

## Playing

Start the engine with `run game` (or simply `run`) to open the relay lobby menu.
Use `run game --solo` for the existing local no-network mode; `run test ...`
continues to run headless and network regression tests.
The public relay now hosts a room directory. Start a relay with
`bin/run relay` and use **Host Game**, **Refresh Games**, and **Join Room** in
two clients without pre-provisioning match IDs or tokens. Private-alpha relays
may optionally require distinct authorized player tokens; see
[pre-match networking](../../doc/code/network/README.md#startup-relay-lobby).

The Host and Join buttons connect to the existing relay transport. Once both
clients are connected, the host clicks **Start Game** (there is no separate
Draw Roles action). The host draws the weighted Hunter/Builder lottery once,
broadcasts the same assignments to the guest, and each client shows a prominent
**five-second role reveal**, including participant names and its own role.
The reveal uses local monotonic time, not simulation time, so it works while
the match is paused and is not extended by world synchronization. Both peers
then bind the guest to the final player slot, transfer an authoritative initial
snapshot and acknowledge its revision.

After the five-second role reveal the dialog automatically disappears.
The **host** resumes authoritative simulation and sends world deltas at about
4 Hz.
Guest Builders, Horses, Towers and unfinished Foundations use exactly the same
nyan-resolved Idle sprite animations as host entities. The guest does not
derive graphics filenames from untrusted network messages or load raw
animation attributes directly (doing so produced red-X placeholder sprites).
Movement currently updates at snapshot/delta cadence; interpolation is future
work. Player-specific recolouring is also deferred.
 The **guest** displays replicated units and sends Move, AttackMove and
BuildTower intents; the host checks player ownership and sends an acceptance
or rejection. Online number-key role switching is disabled. **F10** leaves the
match. After leaving, the engine recreates the world at time zero before
you host or join the next room. Two-player public rooms remain the supported
online configuration. Guest focus-target attacks, queued construction,
foundation prioritization, and recovery snapshots are implemented; complete
online HUD polish and disconnect/reconnect policy are still in progress.

In a Windows development build, run `.\RelWithDebInfo\run.exe game --modpacks fvd` from the build directory with the runtime dependency paths configured.
Selected Builders display their current health above the unit.
The mode uses only these original graphics and the engine API; original game assets are not needed.

The [scenario settings](data/scenario.nyan) define a square 128 by 128 tile map divided into 16 by 16 tile chunks.
Every tile is flat, walkable plains using the generated grass texture.
The FVD renderer draws the exact tile boundaries over the plains for placement reference.
All eight players start with their camera centred at (64, 64).
Player 1 has the Hunter civilisation, starts without units and cannot place buildings.
Players 2 through 8 have the Builders civilisation and each receive exactly one Builder near the centre, with distinct starting positions.
A top-centre countdown shows the seconds remaining until the first Hunter horse, starting at 00:20 when the match begins and disappearing at the first spawn.
It uses the same simulation-time deadline as spawning, so changing the clock speed or pausing the match does not desynchronize the display.
After 20 seconds of match time, the first Hunter horse spawns at the centre of the map; one more spawns every second thereafter.
Horses belong to Hunter player 1 and receive no automatic movement commands.
As each new horse spawns, nearby idle horses ease outwards using deterministic two-dimensional contact separation rather than piling up inside each other.
The crowd push respects blocked tower footprints and reserved construction sites, and can gently move a newly spawned horse away from an obstructed spawn point.
Horses with an active movement order are not redirected by the spawn shove.
The 20-second delay, one-second interval, horse collision radius and shove duration can be changed in the scenario settings.
Horses are controlled by the human Hunter. Select one or drag to select a group, press A or the HUD's Attack-move button, then left-click a destination.
Attack-moving horses detect Builders and completed enemy towers within three tiles and approach to melee range before striking.
Towers always take automatic targeting priority over Builders when both are nearby, even if a Builder is closer.
To override automatic tower priority in local gameplay, select Hunter horses and right-click an enemy Builder or completed tower: the horses focus that exact target until it dies or you give another order.
Right-click empty terrain remains ordinary Move. The FOCUS_ATTACK command is supported by the multiplayer protocol; real-client focus and target-feedback parity remains to be verified.
They resume their original destination after an engagement, or stay there alert for nearby towers after arriving.
If a selected horse dies before a group command is issued, its surviving teammates still carry out the order.
Right-click issues an ordinary Move without seeking enemies; Escape cancels attack-move targeting.
The gather point remains unimplemented.
Completed towers automatically target nearby horses and fire scheduled arrows.
Only authoritative impacts damage health; killing blows credit the tower's Builder.
Tower arrows now appear as brief camera-projected golden streaks with a small ballistic arc, drawn in the QML overlay from the authoritative launch time and flight duration. Visual shots are bounded for large battles, but all damage events still resolve normally.
Shot launch/impact times and trajectory endpoints are already replicated in world snapshots/deltas. The guest renders arrow streaks from those timed events without simulating damage locally; visual parity under latency and recovery still needs real-client verification.
Combat tuning is in scenario.nyan and entity health in entities.nyan.
Online guest sprites now also receive the authoritative position curve's upcoming
keyframes, so known movement routes can play back on the same timeline as the
Hunter rather than being invented from only sampled positions. Until the
curve-aware delta optimization is complete, the host continues to send
authoritative sample corrections as well. See
`openspec/changes/complete-gameplay-replication/` for parity acceptance.
Current sustained-fire target: three towers kill a stationary 90-HP Hunter
horse in roughly 11.4 seconds, four towers in about 9 seconds (2 damage
per arrow, 0.8-second reload, 0.2-second flight). Three Hunter horses
already deal 3 melee damage each, reduced to 2 by tower armour, on a
0.6-second cooldown; they destroy a fully built 100-HP tower in roughly
9.6 seconds of uninterrupted melee. Travel, range, retargeting and
projectile timing can increase actual time-to-kill.
Run `bin/run test openage::gamestate::tests::fvd_combat_balance` to check
the tuned scenario and the live three-horse tower-destruction timeline.
Selecting an owned Tower shows its individual kills; selecting the Builder
shows the lifetime total of all their towers, including ones now destroyed.
Scores are authoritative and per-tower kills replicate only to their owner.

The local view initially controls player 2.
Press keys 1 through 8 to switch the controlled player and return the camera to the common start location.
Drag with the left mouse button to select your Builder and right-click to move it.
The lower-left HUD displays the controlled player and selected entity.
Selecting a tower shows its live HP and its damage and armour fields instead of a build action.
Tower damage and armour read the configured gameplay values, currently 2 arrow damage and 1 armour.
Only a selected Builder can enter tower placement mode, using the B key or Build Tower HUD button.
Select a Builder, click **Build Tower** (or press **B**) and move the cursor to see a footprint preview (green for available, red for occupied, reserved or out of bounds).
Click once to replace the active construction order, or **hold Shift while clicking** to queue multiple sites for the same Builder. **Shift+left-drag** plans a whole line of towers, constrained to the nearer of the two isometric tile axes, starting from the first dragged tile. The full line is submitted as one ordered command so the drag-start foundation is constructed first, with subsequent sites queued even if multiple inputs share a simulation timestamp. Shift-click and Shift-drag keep placement mode active until Esc or right-click. Queue sites carry tiny numbers in build order, visible only to their owning Builder player.
Queued sites reserve their entire footprints immediately; after finishing one tower, the Builder starts the next. Inaccessible queued sites remain as abandoned foundations, and the HUD shows the number waiting.
Right-click or press **Escape** while choosing a tile to cancel placement without moving the Builder.
The old Ctrl-left-click construction shortcut has been removed. Press B or use the Build Tower HUD button instead.
Each queued site displays a persistent, pickable foundation as soon as the order is accepted.
With the Builder selected, right-click one of its own foundations, including an orphaned one, to clear all other construction orders and work on that foundation.
Right-clicking the foundation already being built keeps its progress but cancels the rest of the queue.
Ordinary non-Shift tower placement or moving the Builder to terrain clears the current build queue, releasing its reservations but leaving unfinished foundations orphaned, visible and resumable later by right-clicking them. An interrupted in-progress tower reverts to a foundation.
Footprint preview and queued tower count are displayed in the HUD.
Selecting a Builder with an active order also shows its construction phase.
While walking, the HUD says "Walking to tower site"; while building, it shows a percent-complete progress bar computed from the simulation's own start and finish times.
Canceled or finished jobs clear this progress display.
The panel's height follows its contents, and the actual QML panel geometry blocks world mouse gestures so clicking or dragging its controls never issues a game order.
The simulation validates ownership and reserves the target tile, so the Hunter and other Builders cannot claim the same site.
The Builder walks to an available neighboring tile and constructs for the configured duration.
A foundation marker appears as soon as the order reserves the intended tile, then the tower replaces it when construction starts.
Right-clicking to move interrupts building, and invalid, unreachable, or abandoned construction sites produce no tower.

The bundled modpack lives outside `converted/`.
Its relative graphic references are resolved from the renderer's converted asset directory back to this directory.
For the engine and data format details, see [game simulation](../../doc/code/game_simulation/README.md) and [modpacks](../../doc/media/openage/modpacks.md).

Online guest commands now include right-click Builder focus attacks for
Horses, right-click prioritization of queued Foundations, and Shift+clicked
construction queues. The host validates the target and owner. Guest replication
recovers world-revision gaps with full authenticated snapshots.

After playing, press **F10** to leave and return to a freshly reset FVD
world. The engine now clears renderer state and recreates the event and game
simulation at time zero, rather than requiring a process restart for the next
room. This restart path has not yet been tested on Windows clients. Builder
construction phase/queue and Hunter spawn countdown on the guest HUD now
use the host's replicated state.
