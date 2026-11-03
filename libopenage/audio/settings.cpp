// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "settings.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>

#include "../cvar/cvar.h"

namespace openage::audio {

namespace {

float bus_gain(const Settings &settings, const std::atomic<int> &bus, bool enabled) {
	if (settings.muted.load() or not enabled) {
		return 0.0f;
	}
	return (static_cast<float>(settings.master.load()) / 100.0f)
	       * (static_cast<float>(bus.load()) / 100.0f);
}

} // namespace

float Settings::music_gain() const {
	return bus_gain(*this, this->music, this->music_enabled.load());
}

float Settings::effects_gain() const {
	return bus_gain(*this, this->effects, this->effects_enabled.load());
}

int parse_volume(const std::string &value) {
	if (value.empty()) {
		return -1;
	}
	char *end = nullptr;
	errno = 0;
	const long parsed = std::strtol(value.c_str(), &end, 10);
	if (errno != 0 or end == value.c_str() or *end != '\0') {
		return -1;
	}
	return static_cast<int>(std::clamp(parsed, 0L, 100L));
}

void register_cvars(cvar::CVarManager &cvars, const std::shared_ptr<Settings> &settings) {
	auto bind = [&](const std::string &name, std::atomic<int> Settings::*field) {
		cvars.create(name, {
			[settings, field]() {
				return std::to_string((*settings.*field).load());
			},
			[settings, field](const std::string &value) {
				const int volume = parse_volume(value);
				if (volume >= 0) {
					(*settings.*field).store(volume);
				}
			},
		});
	};
	bind("AUDIO_MASTER_VOLUME", &Settings::master);
	bind("AUDIO_MUSIC_VOLUME", &Settings::music);
	bind("AUDIO_EFFECTS_VOLUME", &Settings::effects);
}

} // namespace openage::audio
