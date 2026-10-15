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
Hunter unit spawning is not implemented yet.

The local view initially controls player 2.
Press keys 1 through 8 to switch the controlled player and return the camera to the common start location.
Drag with the left mouse button to select your Builder, right-click to move it, and Ctrl-left-click to submit a tower build order for a vacant tile.
The simulation validates ownership and reserves the target tile, so the Hunter and other Builders cannot claim the same site.
The Builder walks to an available neighboring tile and constructs for the configured duration (three simulation seconds by default).
A completed tower appears only when construction finishes; the intended tile is reserved meanwhile.
Right-clicking to move interrupts building, and invalid, unreachable, or abandoned construction sites produce no tower.

The bundled modpack lives outside `converted/`.
Its relative graphic references are resolved from the renderer's converted asset directory back to this directory.
For the engine and data format details, see [game simulation](../../doc/code/game_simulation/README.md) and [modpacks](../../doc/media/openage/modpacks.md).
