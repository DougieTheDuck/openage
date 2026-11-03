// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace openage {
namespace cvar {
class CVarManager;
}

namespace audio {

/**
 * Player-facing volume settings, shared between the config system, the
 * presenter (mute key) and the mixer thread. Volumes are percentages 0-100.
 */
struct Settings {
	std::atomic<int> master{100};
	std::atomic<int> music{70};
	std::atomic<int> effects{100};
	std::atomic<bool> muted{false};
	/** Options-menu switches, independent of the volumes. */
	std::atomic<bool> music_enabled{true};
	std::atomic<bool> effects_enabled{true};

	/** Gain 0..1 for the music bus, including master and mute. */
	float music_gain() const;

	/** Gain 0..1 for the effects bus, including master and mute. */
	float effects_gain() const;
};

/**
 * Parse a volume percentage. Returns -1 for anything that isn't an integer;
 * other values are clamped to 0-100.
 */
int parse_volume(const std::string &value);

/**
 * Register AUDIO_MASTER_VOLUME, AUDIO_MUSIC_VOLUME and AUDIO_EFFECTS_VOLUME.
 * Call this before the config files are loaded so audio.oac applies.
 */
void register_cvars(cvar::CVarManager &cvars, const std::shared_ptr<Settings> &settings);

} // namespace audio
} // namespace openage
