# Design notes

## Where sounds come from

- **Hit and death:** the presenter's existing `confirmed_combat_effects` diff
  of successive recipient worlds (host: the local fog-filtered view; guests:
  the replicated world). Fog filtering already happened, so the Hunter never
  hears a hidden Builder die. Only horses, Builders and towers have cues.
- **Foundation placed / tower built:** a diff of
  `make_construction_presentation` between worlds, keyed by owner and tile
  (not entity ID). A new site of the local player's is "placed"; a visible
  site going from not-complete to complete is "built". The first world, a
  reset, a viewer change (solo keys 1-8) and fog memories are silent, so
  revealing an old tower never sounds like a new one. On the host the diff
  runs only when simulation time advances.
- **Order:** the input controller reports right-click orders to the player's
  own horses or Builder, and attack-move/patrol targets.
- **Music:** lobby while the online lobby is open and no match is active,
  otherwise match.

## Engine

The old `libopenage/audio` manager was never finished (no sink feeding, a
16-sample mix buffer, no panning) and nothing used it, so it is left alone.
New, small pieces in the same folder:

- `pcm`: Opus (opusfile, from memory) and WAV (8/16/24-bit, float; mono or
  stereo; any rate, linearly resampled) decoded to 48 kHz stereo int16.
  Music Opus is decoded while it plays rather than up front.
- `mixer`: thread-safe float mix of up to 32 effects plus a looping music
  track with linear crossfade; bus volumes ramp per block to avoid clicks;
  hard clip to int16.
- `output`: a `QIODevice` that renders the mixer, played by `QAudioSink` in
  pull mode with ~100 ms of buffer. Created and destroyed on the presenter
  (Qt GUI) thread. No device, or a failing one, means silence.
- `settings`: atomic volumes registered as cvars before the config loads.
- `fvd_audio`: cues, file discovery, per-cue token-bucket throttles, and
  placement against the camera's view-projection (full volume on screen,
  linear fade to zero half a screen beyond the edge, pan 0.7 x screen x).

## Files

`assets/fvd/sound/<cue>[_n].(opus|wav)` and
`assets/fvd/sound/music/(lobby|match)[_n].(opus|wav)`; numbered variants are
picked at random. See `assets/fvd/sound/README.md` for the cue names.

## Not done

- No 3D distance model beyond the screen, no reverb, no per-sound pitch
  variation.
- Volume settings are config-file only; no in-game options menu yet.
