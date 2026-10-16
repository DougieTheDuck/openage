# FVD prototype assets

This directory contains original development graphics for the FVD tower defense prototype.
The initial set uses a fixed orthographic isometric view, neutral upper-left lighting, transparent sprite backgrounds, and a flat seamless terrain texture.

The runtime files follow the openage [texture](../../doc/media/openage/texture_format_spec.md), [sprite](../../doc/media/openage/sprite_format_spec.md), and [terrain](../../doc/media/openage/terrain_format_spec.md) formats.
The source images were generated with OpenAI's built-in image generation tool and resized for an HD gameplay presentation.
The prompts used for the initial set are recorded in [prompts.md](prompts.md).

## Playing

Start the engine with `run game --modpacks fvd`.
In a Windows development build, run `.\RelWithDebInfo\run.exe game --modpacks fvd` from the build directory with the runtime dependency paths configured.
Selected Builders display their current health above the unit.
The mode uses only these original graphics and the engine API; original game assets are not needed.

The [scenario settings](data/scenario.nyan) define a square 128 by 128 tile map divided into 16 by 16 tile chunks.
Every tile is flat, walkable plains using the generated grass texture.
The FVD renderer draws the exact tile boundaries over the plains for placement reference.
All eight players start with their camera centred at (64, 64).
Player 1 has the Hunter civilisation, starts without units and cannot place buildings.
Players 2 through 8 have the Builders civilisation and each receive exactly one Builder near the centre, with distinct starting positions.
After 20 seconds of match time, the first Hunter horse spawns at the centre of the map; one more spawns every second thereafter.
Horses belong to Hunter player 1 and receive no automatic movement commands.
As each new horse spawns, nearby idle horses ease outwards using deterministic two-dimensional contact separation rather than piling up inside each other.
The crowd push respects blocked tower footprints and reserved construction sites, and can gently move a newly spawned horse away from an obstructed spawn point.
Horses with an active movement order are not redirected by the spawn shove.
The 20-second delay, one-second interval, horse collision radius and shove duration can be changed in the scenario settings.
Horses are controlled by the human Hunter. Select one or drag to select a group, press A or the HUD's Attack-move button, then left-click a destination.
Horses engage Builder units or towers encountered on that route and continue after eliminating the threat.
Right-click issues an ordinary Move without seeking enemies; Escape cancels attack-move targeting.
The gather point remains unimplemented.
Completed towers automatically target nearby horses and fire scheduled arrows.
Only authoritative impacts damage health; killing blows credit the tower's Builder.
Logical arrow impacts are implemented, but visual arrow sprite rendering and network replication remain outstanding.
Combat tuning is in scenario.nyan and the local player's tower kill count appears in the HUD.

The local view initially controls player 2.
Press keys 1 through 8 to switch the controlled player and return the camera to the common start location.
Drag with the left mouse button to select your Builder and right-click to move it.
The lower-left HUD displays the controlled player and selected entity.
Selecting a tower shows its live HP and its damage and armour fields instead of a build action.
Tower damage and armour read the configured gameplay values, currently 8 arrow damage and 1 armour.
Only a selected Builder can enter tower placement mode, including when using the B key or Ctrl-left-click shortcut.
Select a Builder, click **Build Tower** (or press **B**), then click a map tile to place a tower order.
Right-click or press **Escape** while choosing a tile to cancel placement without moving the Builder.
The original Ctrl-left-click build shortcut remains available for development.
Valid/invalid footprint previews and construction progress are not yet displayed in the HUD.
The simulation validates ownership and reserves the target tile, so the Hunter and other Builders cannot claim the same site.
The Builder walks to an available neighboring tile and constructs for the configured duration.
A foundation marker appears as soon as the order reserves the intended tile, then the tower replaces it when construction starts.
Right-clicking to move interrupts building, and invalid, unreachable, or abandoned construction sites produce no tower.

The bundled modpack lives outside `converted/`.
Its relative graphic references are resolved from the renderer's converted asset directory back to this directory.
For the engine and data format details, see [game simulation](../../doc/code/game_simulation/README.md) and [modpacks](../../doc/media/openage/modpacks.md).
