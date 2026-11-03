// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include <chrono>
#include <cmath>
#include <memory>
#include <random>
#include <set>

#include "../testing/testing.h"
#include "fvd_audio.h"

namespace openage::audio::tests {

void fvd_audio_files() {
	TESTEQUALS(cue_for_file("hit.opus").value_or(Cue::COUNT), Cue::HIT);
	TESTEQUALS(cue_for_file("hit_2.wav").value_or(Cue::COUNT), Cue::HIT);
	TESTEQUALS(cue_for_file("death_horse_10.opus").value_or(Cue::COUNT), Cue::DEATH_HORSE);
	TESTEQUALS(cue_for_file("tower_built.opus").value_or(Cue::COUNT), Cue::TOWER_BUILT);
	TESTEQUALS(cue_for_file("death_horse.mp3").has_value(), false);
	TESTEQUALS(cue_for_file("hit_x.opus").has_value(), false);
	TESTEQUALS(cue_for_file("README.md").has_value(), false);
	TESTEQUALS(cue_for_file("lobby.opus").has_value(), false);
	for (size_t i = 0; i < CUE_COUNT; ++i) {
		const auto cue = static_cast<Cue>(i);
		TESTEQUALS(cue_for_file(std::string{cue_name(cue)} + ".opus").value_or(Cue::COUNT), cue);
	}

	TESTEQUALS(is_music_file("lobby.opus", MusicState::LOBBY), true);
	TESTEQUALS(is_music_file("lobby_2.wav", MusicState::LOBBY), true);
	TESTEQUALS(is_music_file("match.opus", MusicState::LOBBY), false);
	TESTEQUALS(is_music_file("match.opus", MusicState::MATCH), true);
	TESTEQUALS(is_music_file("match.opus", MusicState::SILENT), false);
}

void fvd_audio_placement() {
	const Eigen::Matrix4f identity = Eigen::Matrix4f::Identity();
	auto centre = place_on_screen({0.0f, 0.0f, 0.0f}, identity);
	TESTEQUALS(centre.has_value(), true);
	TESTEQUALS(centre->gain, 1.0f);
	TESTEQUALS(centre->pan, 0.0f);

	// On screen near an edge: full volume, panned towards it.
	auto left = place_on_screen({-0.9f, 0.5f, 0.0f}, identity);
	TESTEQUALS(left->gain, 1.0f);
	(left->pan < -0.6f) or TESTFAIL;

	// Just beyond the edge fades; further out is culled.
	auto beyond = place_on_screen({1.2f, 0.0f, 0.0f}, identity);
	(std::abs(beyond->gain - 0.6f) < 1e-4f) or TESTFAIL;
	(std::abs(beyond->pan - 0.7f) < 1e-4f) or TESTFAIL;
	TESTEQUALS(place_on_screen({0.0f, 1.6f, 0.0f}, identity).has_value(), false);

	// Behind the camera or degenerate: never placed.
	Eigen::Matrix4f flipped = Eigen::Matrix4f::Identity();
	flipped(3, 3) = -1.0f;
	TESTEQUALS(place_on_screen({0.0f, 0.0f, 0.0f}, flipped).has_value(), false);
	TESTEQUALS(place_on_screen({NAN, 0.0f, 0.0f}, identity).has_value(), false);
}

void fvd_audio_throttle() {
	using namespace std::chrono_literals;
	const auto start = Throttle::clock::time_point{} + 1h;
	Throttle throttle{3.0f, 4.0f};
	TESTEQUALS(throttle.allow(start), true);
	TESTEQUALS(throttle.allow(start), true);
	TESTEQUALS(throttle.allow(start), true);
	TESTEQUALS(throttle.allow(start), false);
	TESTEQUALS(throttle.allow(start + 100ms), false);
	TESTEQUALS(throttle.allow(start + 260ms), true);
	TESTEQUALS(throttle.allow(start + 260ms), false);
	// Refills to the burst, never beyond it.
	TESTEQUALS(throttle.allow(start + 10s), true);
	TESTEQUALS(throttle.allow(start + 10s), true);
	TESTEQUALS(throttle.allow(start + 10s), true);
	TESTEQUALS(throttle.allow(start + 10s), false);
}

void fvd_audio_bank() {
	SoundBank bank{};
	std::minstd_rand random{7};
	TESTEQUALS(bank.pick(Cue::HIT, random) == nullptr, true);

	auto one = std::make_shared<PcmClip>();
	one->samples = {1, 1};
	auto two = std::make_shared<PcmClip>();
	two->samples = {2, 2};
	bank.add(Cue::HIT, one);
	bank.add(Cue::HIT, two);
	bank.add(Cue::HIT, std::make_shared<PcmClip>());
	TESTEQUALS(bank.variants(Cue::HIT), size_t{2});
	TESTEQUALS(bank.total(), size_t{2});

	std::set<const PcmClip *> seen{};
	for (int i = 0; i < 50; ++i) {
		seen.insert(bank.pick(Cue::HIT, random).get());
	}
	TESTEQUALS(seen.size(), size_t{2});
	TESTEQUALS(bank.pick(Cue::ORDER, random) == nullptr, true);
}

} // namespace openage::audio::tests
