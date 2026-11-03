// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "pcm.h"
#include "settings.h"

namespace openage::audio {

/**
 * Mixes one-shot effects and a looping, crossfading music track into a
 * stereo int16 stream. play_effect()/set_music() are called by the game,
 * render() by the audio output; all methods are thread-safe.
 */
class Mixer {
public:
	/** Effects beyond this many at once are dropped, not queued. */
	static constexpr size_t MAX_EFFECTS = 32;

	explicit Mixer(std::shared_ptr<const Settings> settings);

	/**
	 * Start a one-shot effect.
	 * @param gain 0..1 before the effects/master volume
	 * @param pan -1 (left) .. 1 (right)
	 * @returns false if the voice limit dropped it
	 */
	bool play_effect(std::shared_ptr<const PcmClip> clip, float gain, float pan);

	/**
	 * Crossfade to a looping track. The current track fades out over the
	 * same time. A null stream just fades the music out.
	 */
	void set_music(std::unique_ptr<Stream> stream, float fade_seconds);

	/** Fill `frames` interleaved stereo frames. */
	void render(int16_t *out, size_t frames);

	/** Effects still sounding (for tests and diagnostics). */
	size_t active_effects() const;

	/** Whether any music track (fading or not) is playing. */
	bool music_playing() const;

private:
	struct Voice {
		std::shared_ptr<const PcmClip> clip;
		size_t position;
		float left;
		float right;
	};

	struct Track {
		std::unique_ptr<Stream> stream;
		/** Gain now, and how much it changes each frame (+ fade in, - out). */
		float gain;
		float step;
		/** Set when a loop restart produced nothing (empty/broken file). */
		bool exhausted = false;
	};

	void mix_track(Track &track, float *mix, size_t frames, float bus_from, float bus_to);

	std::shared_ptr<const Settings> settings;

	mutable std::mutex mutex;
	std::vector<Voice> voices;
	std::vector<Track> tracks;

	/** Bus gains applied in the previous render, ramped to avoid clicks. */
	float applied_music = 0.0f;
	float applied_effects = 0.0f;

	std::vector<float> mix_buffer;
	std::vector<int16_t> read_buffer;
};

} // namespace openage::audio
