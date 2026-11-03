// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "fvd_audio.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

#include "../log/log.h"
#include "../util/file.h"
#include "../util/path.h"
#include "error.h"
#include "mixer.h"
#include "output.h"

namespace openage::audio {

namespace {

struct CueRule {
	const char *name;
	/** Level before the effects/master volume. */
	float gain;
	/** Throttle burst and sustained rate. */
	float burst;
	float per_second;
};

constexpr std::array<CueRule, CUE_COUNT> RULES{{
	{"hit", 0.6f, 4.0f, 10.0f},
	{"death_horse", 0.9f, 3.0f, 4.0f},
	{"death_builder", 0.9f, 3.0f, 4.0f},
	{"death_tower", 0.8f, 3.0f, 4.0f},
	{"foundation_placed", 0.5f, 3.0f, 8.0f},
	{"tower_built", 0.7f, 3.0f, 4.0f},
	{"order", 0.6f, 1.0f, 5.0f},
}};

constexpr float MUSIC_FADE_SECONDS = 2.0f;

/** How far pan follows the screen position; full hard-panning is tiring. */
constexpr float PAN_WIDTH = 0.7f;

/** Split "name_3.opus" into stem "name" when the part after '_' is a number. */
std::optional<std::string> sound_stem(const std::string &file_name) {
	if (not is_supported_sound_file(file_name)) {
		return std::nullopt;
	}
	std::string stem = file_name.substr(0, file_name.rfind('.'));
	const auto underscore = stem.rfind('_');
	if (underscore != std::string::npos and underscore + 1 < stem.size()
	    and std::all_of(stem.begin() + underscore + 1, stem.end(), [](unsigned char c) {
		    return std::isdigit(c);
	    })) {
		stem.resize(underscore);
	}
	return stem;
}

const char *music_stem(MusicState state) {
	switch (state) {
	case MusicState::LOBBY:
		return "lobby";
	case MusicState::MATCH:
		return "match";
	default:
		return nullptr;
	}
}

} // namespace

const char *cue_name(Cue cue) {
	return RULES.at(static_cast<size_t>(cue)).name;
}

std::optional<Cue> cue_for_file(const std::string &file_name) {
	const auto stem = sound_stem(file_name);
	if (not stem) {
		return std::nullopt;
	}
	for (size_t i = 0; i < CUE_COUNT; ++i) {
		if (*stem == RULES[i].name) {
			return static_cast<Cue>(i);
		}
	}
	return std::nullopt;
}

bool is_music_file(const std::string &file_name, MusicState state) {
	const auto stem = sound_stem(file_name);
	const char *wanted = music_stem(state);
	return stem and wanted and *stem == wanted;
}

std::optional<Placement> place_on_screen(const Eigen::Vector3f &world,
                                         const Eigen::Matrix4f &view_projection) {
	const Eigen::Vector4f clip = view_projection * Eigen::Vector4f{world.x(), world.y(), world.z(), 1.0f};
	if (not std::isfinite(clip.w()) or clip.w() <= 0.0f
	    or not std::isfinite(clip.x()) or not std::isfinite(clip.y())) {
		return std::nullopt;
	}
	const float x = clip.x() / clip.w();
	const float y = clip.y() / clip.w();
	const float outside = std::max(std::abs(x), std::abs(y)) - 1.0f;
	const float gain = outside <= 0.0f ? 1.0f : 1.0f - outside / EDGE_FALLOFF;
	if (gain <= 0.0f) {
		return std::nullopt;
	}
	return Placement{gain, std::clamp(x, -1.0f, 1.0f) * PAN_WIDTH};
}

Throttle::Throttle(float capacity, float per_second) :
	capacity{capacity},
	per_second{per_second},
	tokens{capacity} {}

bool Throttle::allow(clock::time_point now) {
	if (this->last) {
		const float elapsed = std::chrono::duration<float>(now - *this->last).count();
		this->tokens = std::min(this->capacity, this->tokens + std::max(0.0f, elapsed) * this->per_second);
	}
	this->last = now;
	if (this->tokens < 1.0f) {
		return false;
	}
	this->tokens -= 1.0f;
	return true;
}

void SoundBank::add(Cue cue, std::shared_ptr<const PcmClip> clip) {
	if (clip and clip->frames() > 0) {
		this->clips.at(static_cast<size_t>(cue)).push_back(std::move(clip));
	}
}

std::shared_ptr<const PcmClip> SoundBank::pick(Cue cue, std::minstd_rand &random) const {
	const auto &options = this->clips.at(static_cast<size_t>(cue));
	if (options.empty()) {
		return nullptr;
	}
	std::uniform_int_distribution<size_t> choose{0, options.size() - 1};
	return options[choose(random)];
}

size_t SoundBank::variants(Cue cue) const {
	return this->clips.at(static_cast<size_t>(cue)).size();
}

size_t SoundBank::total() const {
	size_t count = 0;
	for (const auto &options : this->clips) {
		count += options.size();
	}
	return count;
}

FvdAudio::FvdAudio(const util::Path &sound_dir, std::shared_ptr<Settings> settings) :
	settings{std::move(settings)},
	random{std::random_device{}()} {
	for (const auto &rule : RULES) {
		this->throttles.emplace_back(rule.burst, rule.per_second);
	}

	size_t music_files = 0;
	util::Path dir = sound_dir;
	try {
		if (not dir.is_dir()) {
			log::log(INFO << "Audio: no sound folder at " << sound_dir << ", playing silently");
			return;
		}
		for (auto &entry : dir.iterdir()) {
			const auto &name = entry.get_name();
			const auto cue = cue_for_file(name);
			if (not cue or not entry.is_file()) {
				continue;
			}
			try {
				this->bank.add(*cue, decode_clip(entry.open_r().read(), name));
			}
			catch (const std::exception &error) {
				log::log(WARN << "Audio: skipping " << name << ": " << error.what());
			}
		}
		auto music_dir = dir / "music";
		if (music_dir.is_dir()) {
			for (auto &entry : music_dir.iterdir()) {
				const auto &name = entry.get_name();
				for (auto state : {MusicState::LOBBY, MusicState::MATCH}) {
					if (is_music_file(name, state) and entry.is_file()) {
						this->music.at(static_cast<size_t>(state)).emplace_back(name, entry.open_r().read());
						++music_files;
					}
				}
			}
		}
	}
	catch (const std::exception &error) {
		log::log(WARN << "Audio: could not read " << sound_dir << ": " << error.what());
	}

	if (this->bank.total() == 0 and music_files == 0) {
		log::log(INFO << "Audio: no sound files in " << sound_dir << ", playing silently");
		return;
	}
	log::log(INFO << "Audio: loaded " << this->bank.total() << " effects and "
	              << music_files << " music tracks");

	this->mixer = std::make_shared<Mixer>(this->settings);
	this->output = Output::open(this->mixer);
	if (not this->output) {
		this->mixer.reset();
	}
}

FvdAudio::~FvdAudio() = default;

bool FvdAudio::active() const {
	return this->mixer != nullptr;
}

void FvdAudio::set_music(MusicState state) {
	if (state == this->music_state) {
		return;
	}
	this->music_state = state;
	if (not this->active()) {
		return;
	}
	std::unique_ptr<Stream> stream{};
	if (state != MusicState::SILENT) {
		const auto &tracks = this->music.at(static_cast<size_t>(state));
		if (not tracks.empty()) {
			std::uniform_int_distribution<size_t> choose{0, tracks.size() - 1};
			const auto &[name, bytes] = tracks[choose(this->random)];
			try {
				stream = open_stream(bytes, name);
			}
			catch (const std::exception &error) {
				log::log(WARN << "Audio: cannot play " << name << ": " << error.what());
			}
		}
	}
	this->mixer->set_music(std::move(stream), MUSIC_FADE_SECONDS);
}

void FvdAudio::set_view(const Eigen::Matrix4f &view_projection) {
	this->view = view_projection;
}

void FvdAudio::play(Cue cue) {
	this->play_placed(cue, 1.0f, 0.0f);
}

void FvdAudio::play_at(Cue cue, const Eigen::Vector3f &world) {
	if (not this->active() or not this->view) {
		return;
	}
	const auto placement = place_on_screen(world, *this->view);
	if (placement) {
		this->play_placed(cue, placement->gain, placement->pan);
	}
}

void FvdAudio::play_placed(Cue cue, float gain, float pan) {
	if (not this->active()) {
		return;
	}
	const auto index = static_cast<size_t>(cue);
	auto clip = this->bank.pick(cue, this->random);
	if (not clip or not this->throttles.at(index).allow(Throttle::clock::now())) {
		return;
	}
	this->mixer->play_effect(std::move(clip), gain * RULES.at(index).gain, pan);
}

bool FvdAudio::toggle_mute() {
	const bool muted = not this->settings->muted.load();
	this->settings->muted.store(muted);
	log::log(INFO << "Audio: " << (muted ? "muted" : "unmuted"));
	return muted;
}

} // namespace openage::audio
