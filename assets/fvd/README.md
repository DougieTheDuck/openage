# FVD prototype assets

This directory contains original development graphics for the FVD tower defense prototype.
The initial set uses a fixed orthographic isometric view, neutral upper-left lighting, transparent sprite backgrounds, and a flat seamless terrain texture.

The runtime files follow the openage [texture](../../doc/media/openage/texture_format_spec.md), [sprite](../../doc/media/openage/sprite_format_spec.md), and [terrain](../../doc/media/openage/terrain_format_spec.md) formats.
The source images were generated with OpenAI's built-in image generation tool and resized for an HD gameplay presentation.
The prompts used for the initial set are recorded in [prompts.md](prompts.md).

## Playing

Start the engine with `run game` (or simply `run`) to open the relay lobby menu.
Use `run game --solo` for the existing local no-network mode; `run test ...`
continues to run headless and network regression tests.
The relay menu requires pre-provisioned relay credentials; the current relay
server cannot discover a room or mint credentials in the menu. See
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

**The current screen is a synchronized bootstrap, not yet playable online
gameplay**: after the five-second reveal the menu displays the real bootstrap
status instead of starting two independently simulated games. Continuous
command/replication/rendering handoff remains to be implemented. See the
relay developer test in the networking documentation.

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
Right-click empty terrain remains ordinary Move. Focus-attack has not yet been added to the multiplayer command protocol.
They resume their original destination after an engagement, or stay there alert for nearby towers after arriving.
If a selected horse dies before a group command is issued, its surviving teammates still carry out the order.
Right-click issues an ordinary Move without seeking enemies; Escape cancels attack-move targeting.
The gather point remains unimplemented.
Completed towers automatically target nearby horses and fire scheduled arrows.
Only authoritative impacts damage health; killing blows credit the tower's Builder.
Tower arrows now appear as brief camera-projected golden streaks with a small ballistic arc, drawn in the QML overlay from the authoritative launch time and flight duration. Visual shots are bounded for large battles, but all damage events still resolve normally.
Network replication of those shot visuals remains outstanding; this overlay currently follows only the local authoritative simulation.
Combat tuning is in scenario.nyan.
Selecting an owned Tower shows its individual kills; selecting the Builder
shows the lifetime total of all their towers, including ones now destroyed.
Scores are authoritative and per-tower kills replicate only to their owner.

The local view initially controls player 2.
Press keys 1 through 8 to switch the controlled player and return the camera to the common start location.
Drag with the left mouse button to select your Builder and right-click to move it.
The lower-left HUD displays the controlled player and selected entity.
Selecting a tower shows its live HP and its damage and armour fields instead of a build action.
Tower damage and armour read the configured gameplay values, currently 8 arrow damage and 1 armour.
Only a selected Builder can enter tower placement mode, using the B key or Build Tower HUD button.
Select a Builder, click **Build Tower** (or press **B**) and move the cursor to see a footprint preview (green for available, red for occupied, reserved or out of bounds).
Click once to replace the active construction order, or **hold Shift while clicking** to queue multiple sites for the same Builder. Shift-click keeps placement mode active until Esc or right-click.
Queued sites reserve their entire footprints immediately; after finishing one tower, the Builder starts the next. Inaccessible queued sites are skipped and the HUD shows the number waiting.
Right-click or press **Escape** while choosing a tile to cancel placement without moving the Builder.
The old Ctrl-left-click construction shortcut has been removed. Press B or use the Build Tower HUD button instead.
Each queued site displays a persistent, pickable foundation as soon as the order is accepted.
With the Builder selected, right-click one of its own foundations to clear all other construction orders and immediately work on that foundation.
Right-clicking the foundation already being built keeps its progress but cancels the rest of the queue.
Moving the Builder to ordinary terrain cancels all outstanding foundations and reservations.
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
