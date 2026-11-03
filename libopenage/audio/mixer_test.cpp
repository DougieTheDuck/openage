// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../testing/testing.h"
#include "error.h"
#include "mixer.h"
#include "pcm.h"
#include "settings.h"

namespace openage::audio::tests {

namespace {

std::shared_ptr<const PcmClip> constant_clip(int16_t left, int16_t right, size_t frames) {
	auto clip = std::make_shared<PcmClip>();
	for (size_t i = 0; i < frames; ++i) {
		clip->samples.push_back(left);
		clip->samples.push_back(right);
	}
	return clip;
}

bool near(int value, int expected, int tolerance = 2) {
	return std::abs(value - expected) <= tolerance;
}

void put_u32(std::string &out, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
	}
}

void put_u16(std::string &out, uint16_t value) {
	out.push_back(static_cast<char>(value & 0xFF));
	out.push_back(static_cast<char>(value >> 8));
}

std::string wav(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits, const std::string &data) {
	std::string out = "RIFF";
	put_u32(out, static_cast<uint32_t>(36 + data.size()));
	out += "WAVEfmt ";
	put_u32(out, 16);
	put_u16(out, format);
	put_u16(out, channels);
	put_u32(out, rate);
	put_u32(out, rate * channels * bits / 8);
	put_u16(out, static_cast<uint16_t>(channels * bits / 8));
	put_u16(out, bits);
	out += "data";
	put_u32(out, static_cast<uint32_t>(data.size()));
	return out + data;
}

} // namespace

void audio_settings() {
	TESTEQUALS(parse_volume("0"), 0);
	TESTEQUALS(parse_volume("55"), 55);
	TESTEQUALS(parse_volume("250"), 100);
	TESTEQUALS(parse_volume("-3"), 0);
	TESTEQUALS(parse_volume("loud"), -1);
	TESTEQUALS(parse_volume("5x"), -1);
	TESTEQUALS(parse_volume(""), -1);

	Settings settings{};
	settings.master = 50;
	settings.music = 50;
	settings.effects = 100;
	near(static_cast<int>(settings.music_gain() * 100), 25, 0) or TESTFAIL;
	near(static_cast<int>(settings.effects_gain() * 100), 50, 0) or TESTFAIL;
	settings.muted = true;
	TESTEQUALS(settings.music_gain(), 0.0f);
	TESTEQUALS(settings.effects_gain(), 0.0f);
}

void audio_mixer_effects() {
	auto settings = std::make_shared<Settings>();
	Mixer mixer{settings};
	std::vector<int16_t> out(200 * 2);

	// Silence when nothing plays.
	mixer.render(out.data(), 200);
	for (auto sample : out) {
		TESTEQUALS(sample, 0);
	}

	// Centre: full level in both ears. Ends after the clip, and is removed.
	TESTEQUALS(mixer.play_effect(constant_clip(16384, 16384, 100), 1.0f, 0.0f), true);
	mixer.render(out.data(), 200);
	near(out[0], 16384) or TESTFAIL;
	near(out[1], 16384) or TESTFAIL;
	near(out[99 * 2], 16384) or TESTFAIL;
	TESTEQUALS(out[100 * 2], 0);
	TESTEQUALS(mixer.active_effects(), size_t{0});

	// Hard right: nothing on the left. Half gain halves the level.
	mixer.play_effect(constant_clip(16384, 16384, 100), 0.5f, 1.0f);
	mixer.render(out.data(), 50);
	TESTEQUALS(out[0], 0);
	near(out[1], 8192) or TESTFAIL;
	TESTEQUALS(mixer.active_effects(), size_t{1});

	// Overlapping sounds sum and clip rather than wrap.
	mixer.play_effect(constant_clip(30000, 30000, 100), 1.0f, 0.0f);
	mixer.play_effect(constant_clip(30000, 30000, 100), 1.0f, 0.0f);
	mixer.render(out.data(), 10);
	TESTEQUALS(out[0], 32767);

	// Silent, empty or voice-limited effects are dropped.
	TESTEQUALS(mixer.play_effect(constant_clip(1, 1, 10), 0.0f, 0.0f), false);
	TESTEQUALS(mixer.play_effect(std::make_shared<PcmClip>(), 1.0f, 0.0f), false);
	for (size_t i = 0; i < Mixer::MAX_EFFECTS; ++i) {
		mixer.play_effect(constant_clip(1, 1, 1000), 1.0f, 0.0f);
	}
	TESTEQUALS(mixer.active_effects(), Mixer::MAX_EFFECTS);
	TESTEQUALS(mixer.play_effect(constant_clip(1, 1, 1000), 1.0f, 0.0f), false);
}

void audio_mixer_volume() {
	auto settings = std::make_shared<Settings>();
	Mixer mixer{settings};
	std::vector<int16_t> out(400 * 2);

	// Muting ramps down over one block, then stays silent.
	settings->muted = true;
	mixer.play_effect(constant_clip(16384, 16384, 1000), 1.0f, 0.0f);
	mixer.render(out.data(), 400);
	near(out[0], 16384) or TESTFAIL;
	(out[399 * 2] < 100) or TESTFAIL;
	mixer.render(out.data(), 400);
	for (auto sample : out) {
		TESTEQUALS(sample, 0);
	}

	// The effects volume scales effects.
	settings->muted = false;
	settings->effects = 25;
	mixer.play_effect(constant_clip(16384, 16384, 2000), 1.0f, 0.0f);
	mixer.render(out.data(), 400);
	mixer.render(out.data(), 100);
	near(out[0], 4096) or TESTFAIL;
}

void audio_mixer_music() {
	auto settings = std::make_shared<Settings>();
	settings->music = 100;
	Mixer mixer{settings};
	std::vector<int16_t> out(1000 * 2);

	// A short track loops seamlessly once faded in.
	mixer.set_music(std::make_unique<ClipStream>(constant_clip(10000, 10000, 70)), 0.001f);
	mixer.render(out.data(), 1000);
	near(out[0], 0, 300) or TESTFAIL;
	near(out[500 * 2], 10000) or TESTFAIL;
	near(out[999 * 2 + 1], 10000) or TESTFAIL;
	TESTEQUALS(mixer.music_playing(), true);

	// Crossfade: both play during the fade, then only the new one.
	mixer.set_music(std::make_unique<ClipStream>(constant_clip(-8000, -8000, 50)), 0.01f);
	mixer.render(out.data(), 1000);
	(out[240 * 2] > -8000 and out[240 * 2] < 10000) or TESTFAIL;
	near(out[999 * 2], -8000) or TESTFAIL;

	// Fading to nothing stops the music.
	mixer.set_music(nullptr, 0.01f);
	mixer.render(out.data(), 1000);
	TESTEQUALS(out[999 * 2], 0);
	TESTEQUALS(mixer.music_playing(), false);

	// An empty track is dropped instead of spinning forever.
	mixer.set_music(std::make_unique<ClipStream>(std::make_shared<PcmClip>()), 0.001f);
	mixer.render(out.data(), 100);
	TESTEQUALS(mixer.music_playing(), false);
}

void audio_decode_wav() {
	// 16-bit mono at 24 kHz: resampled to 48 kHz stereo.
	std::string pcm{};
	for (int i = 0; i < 100; ++i) {
		put_u16(pcm, static_cast<uint16_t>(int16_t{8192}));
	}
	auto clip = decode_clip(wav(1, 1, 24000, 16, pcm), "hit.wav");
	near(static_cast<int>(clip->frames()), 199, 1) or TESTFAIL;
	near(clip->samples[0], 8192) or TESTFAIL;
	near(clip->samples[1], 8192) or TESTFAIL;

	// 32-bit float stereo at 48 kHz keeps both channels.
	std::string floats{};
	for (float value : {0.5f, -0.25f}) {
		uint32_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		put_u32(floats, bits);
	}
	clip = decode_clip(wav(3, 2, 48000, 32, floats), "x.WAV");
	TESTEQUALS(clip->frames(), size_t{1});
	near(clip->samples[0], 16384) or TESTFAIL;
	near(clip->samples[1], -8192) or TESTFAIL;

	TESTTHROWS(decode_clip("not a wav", "hit.wav"));
	TESTTHROWS(decode_clip(wav(1, 6, 48000, 16, pcm), "surround.wav"));
	TESTTHROWS(decode_clip("OggS but not really", "hit.opus"));
	TESTTHROWS(decode_clip("", "hit.mp3"));
	TESTEQUALS(is_supported_sound_file("Hit.OPUS"), true);
	TESTEQUALS(is_supported_sound_file("hit.ogg"), false);
}

} // namespace openage::audio::tests
