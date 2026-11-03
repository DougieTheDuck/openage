# FVD sounds

The game plays whatever it finds here. No real sounds ship yet.

## Placeholders (temporary)

Until every cue has a recording, the game generates simple synthesised
placeholder sounds at startup (solo, host and guest alike) with
`openage/game/placeholder_sounds.py`. They go to a per-user cache folder
(`~/.cache/openage/fvd-placeholder-sounds-v1` on Linux, the openage data
folder on Windows), never into this folder, and only missing files are
written, so it takes about two seconds once and nothing after that.

A real file here always wins: a placeholder is only used for a cue (or music
state) that has no file in this folder. The log says how many cues fell back.
`--no-audio` skips the generator.

To remove the placeholders once all sounds exist: delete
`openage/game/placeholder_sounds.py` (and its entry in
`openage/game/CMakeLists.txt`), the `ensure_placeholder_sounds` block in
`openage/game/main.py` and `openage/main/main.py`, and the
`fvd_placeholder_sounds` fallback argument in `Presenter::init_audio`.

Formats: Opus (`.opus`) or WAV (`.wav`, 8/16/24-bit or float, mono or stereo,
any sample rate). Add variants with a number, e.g. `hit.opus`, `hit_2.opus`,
`hit_3.opus`; one is picked at random each time.

## Effects

| File stem           | When                                                     | Heard by          |
|---------------------|----------------------------------------------------------|-------------------|
| `hit`               | a horse, Builder or tower loses HP                       | anyone who sees it |
| `death_horse`       | a horse dies                                             | anyone who sees it |
| `death_builder`     | a Builder dies                                           | anyone who sees it |
| `death_tower`       | a tower is destroyed or removed                          | anyone who sees it |
| `foundation_placed` | you place one of your own foundations                    | you only          |
| `tower_built`       | a tower you can see finishes construction                | anyone who sees it |
| `order`             | you give your own units a move/attack/patrol order       | you only          |
| `hunter_unleashed`  | the Hunter's horses start spawning (with a big banner)   | everyone          |
| `flare`             | a Builder flares (the "gling")                           | all Builders      |

World sounds are full volume on screen, fade out just beyond the screen edge,
and pan left/right with their position. Bursts are rate-limited per sound.

## Music

`music/lobby.opus` plays in the lobby and `music/match.opus` during a match
(also `.wav`, and numbered variants). Tracks loop and crossfade over 2 seconds.
Prefer Opus for music: it is decoded while playing instead of all at once.

## Settings

`cfg/audio.oac` sets `AUDIO_MASTER_VOLUME`, `AUDIO_MUSIC_VOLUME` and
`AUDIO_EFFECTS_VOLUME` (0-100). M mutes during a match. `--no-audio` never
opens an audio device.
