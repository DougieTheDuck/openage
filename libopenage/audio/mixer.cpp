// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "mixer.h"

#include <algorithm>
#include <cmath>

namespace openage::audio {

Mixer::Mixer(std::shared_ptr<const Settings> settings) :
	settings{std::move(settings)} {
	this->applied_music = this->settings->music_gain();
	this->applied_effects = this->settings->effects_gain();
}

bool Mixer::play_effect(std::shared_ptr<const PcmClip> clip, float gain, float pan) {
	if (not clip or clip->frames() == 0 or not(gain > 0.0f)) {
		return false;
	}
	gain = std::min(gain, 1.0f);
	pan = std::clamp(pan, -1.0f, 1.0f);
	std::scoped_lock lock{this->mutex};
	if (this->voices.size() >= MAX_EFFECTS) {
		return false;
	}
	// Balance pan: the centre keeps full level in both ears.
	this->voices.push_back({std::move(clip), 0, gain * std::min(1.0f, 1.0f - pan),
	                        gain * std::min(1.0f, 1.0f + pan)});
	return true;
}

void Mixer::set_music(std::unique_ptr<Stream> stream, float fade_seconds) {
	const float fade_frames = std::max(1.0f, fade_seconds * OUTPUT_RATE);
	std::scoped_lock lock{this->mutex};
	// Only one track fades out at a time; anything older is dropped.
	std::erase_if(this->tracks, [](const Track &track) { return track.step < 0.0f; });
	for (auto &track : this->tracks) {
		track.step = -std::max(track.gain, 1e-6f) / fade_frames;
	}
	if (stream) {
		this->tracks.push_back({std::move(stream), 0.0f, 1.0f / fade_frames});
	}
}

void Mixer::mix_track(Track &track, float *mix, size_t frames, float bus_from, float bus_to) {
	this->read_buffer.resize(frames * OUTPUT_CHANNELS);
	size_t done = 0;
	while (done < frames and not track.exhausted) {
		const size_t got = track.stream->read(this->read_buffer.data(), frames - done);
		if (got == 0) {
			// Loop. A second empty read in a row means the track is broken.
			track.stream->rewind();
			if (track.stream->read(this->read_buffer.data(), 1) == 0) {
				track.exhausted = true;
				break;
			}
			track.stream->rewind();
			continue;
		}
		for (size_t i = 0; i < got; ++i) {
			const size_t frame = done + i;
			track.gain = std::clamp(track.gain + track.step, 0.0f, 1.0f);
			const float bus = bus_from + (bus_to - bus_from) * (static_cast<float>(frame) / frames);
			const float gain = track.gain * bus / 32768.0f;
			mix[frame * 2] += this->read_buffer[i * 2] * gain;
			mix[frame * 2 + 1] += this->read_buffer[i * 2 + 1] * gain;
		}
		done += got;
	}
}

void Mixer::render(int16_t *out, size_t frames) {
	const float music_target = this->settings->music_gain();
	const float effects_target = this->settings->effects_gain();

	std::scoped_lock lock{this->mutex};
	this->mix_buffer.assign(frames * OUTPUT_CHANNELS, 0.0f);
	float *mix = this->mix_buffer.data();

	for (auto &track : this->tracks) {
		this->mix_track(track, mix, frames, this->applied_music, music_target);
	}
	std::erase_if(this->tracks, [](const Track &track) {
		return track.exhausted or (track.step < 0.0f and track.gain <= 0.0f);
	});

	for (auto &voice : this->voices) {
		const auto &samples = voice.clip->samples;
		const size_t count = std::min(frames, voice.clip->frames() - voice.position);
		for (size_t i = 0; i < count; ++i) {
			const float bus = this->applied_effects
			                  + (effects_target - this->applied_effects) * (static_cast<float>(i) / frames);
			const size_t source = (voice.position + i) * 2;
			mix[i * 2] += samples[source] * voice.left * bus / 32768.0f;
			mix[i * 2 + 1] += samples[source + 1] * voice.right * bus / 32768.0f;
		}
		voice.position += count;
	}
	std::erase_if(this->voices, [](const Voice &voice) {
		return voice.position >= voice.clip->frames();
	});

	this->applied_music = music_target;
	this->applied_effects = effects_target;

	for (size_t i = 0; i < frames * OUTPUT_CHANNELS; ++i) {
		out[i] = static_cast<int16_t>(std::lround(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f));
	}
}

size_t Mixer::active_effects() const {
	std::scoped_lock lock{this->mutex};
	return this->voices.size();
}

bool Mixer::music_playing() const {
	std::scoped_lock lock{this->mutex};
	return not this->tracks.empty();
}

} // namespace openage::audio
