// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <chrono>
#include <optional>

#include "time/time.h"

namespace openage::presenter {

/** Presentation time for interpolating received worlds without advancing the simulation. */
class GuestClock {
public:
	/**
	 * Accept a host sample and return the preceding sample time, if any.
	 * Recovery snapshots reset interpolation instead of blending across missing state.
	 * Playback buffers at least half an observed sample interval before starting or after starvation.
	 * Delivery jitter can increase this reserve, capped at one sample interval.
	 * Its rate follows observed host clock drift, with small buffer corrections.
	 * Lag exceeding four observed intervals resets presentation to the current sample.
	 */
	std::optional<time::time_t> update(time::time_t sample_time,
	                                   std::chrono::steady_clock::time_point now,
	                                   bool reset);

	/** Advance with wall time, stopping at the latest authoritative sample. */
	time::time_t get_time(std::chrono::steady_clock::time_point now) const;

private:
	/** Retain a half-interval safety margin in addition to observed jitter. */
	time::time_t get_reserve() const;

	std::optional<time::time_t> latest_sample;
	time::time_t playback_start{0};
	time::time_t playback_rate{1};
	time::time_t sample_interval{0};
	time::time_t arrival_interval{0};
	time::time_t arrival_jitter{0};
	std::optional<time::time_t> cadence_start;
	std::chrono::steady_clock::time_point cadence_origin{};
	std::chrono::steady_clock::time_point playback_origin{};
	std::chrono::steady_clock::time_point last_arrival{};
};

} // namespace openage::presenter
