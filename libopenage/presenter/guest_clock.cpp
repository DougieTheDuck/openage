// Copyright 2026 the openage authors. See copying.md for legal info.

#include "guest_clock.h"

#include <algorithm>

#include "error/error.h"

namespace openage::presenter {

namespace {

time::time_t wall_time_between(std::chrono::steady_clock::time_point from,
                               std::chrono::steady_clock::time_point to) {
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
	return time::time_t::from_int(elapsed) / 1000000;
}

} // namespace

std::optional<time::time_t> GuestClock::update(time::time_t sample_time,
                                               std::chrono::steady_clock::time_point now,
                                               bool reset) {
	auto previous = this->latest_sample;
	if (reset or not previous) {
		this->playback_start = sample_time;
		this->playback_origin = now;
		this->playback_rate = 1;
		this->sample_interval = 0;
		this->arrival_interval = 0;
		this->arrival_jitter = 0;
		this->cadence_start.reset();
		previous.reset();
	}
	else {
		if (sample_time < *previous) {
			throw Error{MSG(err) << "FVD guest sample time moved backwards"};
		}
		if (this->sample_interval > time::TIME_ZERO
		    and sample_time - this->get_time(now) > this->sample_interval * 4) {
			// Old presentation data must not keep fresh commands behind a long catch-up queue.
			return this->update(sample_time, now, true);
		}
		this->playback_start = this->get_time(now);
		this->playback_origin = std::max(now, this->playback_origin);
		const auto advance = sample_time - *previous;
		if (advance > time::TIME_ZERO) {
			const auto elapsed = wall_time_between(this->last_arrival, now);
			if (not this->cadence_start or elapsed > this->arrival_interval * 2) {
				this->cadence_start = sample_time;
				this->cadence_origin = now;
			}
			const auto cadence_elapsed = wall_time_between(this->cadence_origin, now);
			const auto source_rate = this->arrival_interval > time::TIME_ZERO
			                                 and cadence_elapsed >= this->arrival_interval * 4 ?
			                             (sample_time - *this->cadence_start) / cadence_elapsed :
			                             time::time_t::from_int(1);
			if (this->arrival_interval > time::TIME_ZERO and elapsed <= this->arrival_interval * 2) {
				const auto jitter = elapsed > this->arrival_interval ? elapsed - this->arrival_interval : this->arrival_interval - elapsed;
				this->arrival_jitter = std::min(this->sample_interval,
				                                std::max(jitter, this->arrival_jitter * 31 / 32));
			}
			if (this->sample_interval == time::TIME_ZERO or this->playback_start == *previous) {
				// Leave room for late packets before consuming the first available segment.
				this->sample_interval = advance;
				this->arrival_interval = advance;
				this->playback_rate = source_rate;
				const auto reserve = this->get_reserve();
				this->playback_origin = now + std::chrono::microseconds{(reserve / source_rate * 1000000).to_int()};
			}
			else {
				if (elapsed > time::TIME_ZERO) {
					this->sample_interval = (this->sample_interval * 7 + advance) / 8;
					this->arrival_interval = (this->arrival_interval * 7 + elapsed) / 8;
				}
				// Estimate clock drift across the stream, not from individual packet gaps.
				this->playback_rate = source_rate;
				const auto buffered = sample_time - this->playback_start;
				const auto reserve = this->get_reserve();
				// A dead band keeps ordinary arrival variation from changing the playback rate.
				if (buffered < this->sample_interval * 3 / 4 + reserve) {
					this->playback_rate = this->playback_rate * 49 / 50;
				}
				else if (buffered > this->sample_interval * 5 / 4 + reserve) {
					this->playback_rate = this->playback_rate * 51 / 50;
				}
			}
		}
	}
	if (not previous or sample_time > *previous) {
		this->last_arrival = now;
	}
	this->latest_sample = sample_time;
	return previous;
}

time::time_t GuestClock::get_reserve() const {
	return std::min(this->sample_interval, this->sample_interval / 2 + this->arrival_jitter);
}

time::time_t GuestClock::get_time(std::chrono::steady_clock::time_point now) const {
	if (not this->latest_sample) {
		return time::TIME_ZERO;
	}
	const auto elapsed = std::max(time::TIME_ZERO, wall_time_between(this->playback_origin, now));
	return std::clamp(this->playback_start + elapsed * this->playback_rate,
	                  this->playback_start,
	                  *this->latest_sample);
}

} // namespace openage::presenter
