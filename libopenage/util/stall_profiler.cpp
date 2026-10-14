// Copyright 2026 the openage authors. See copying.md for legal info.

#include "stall_profiler.h"

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <system_error>

#include "log/log.h"


namespace openage::util {

namespace {

constexpr time_nsec_t NANOSECONDS_PER_MILLISECOND = 1000000;

time_nsec_t get_stall_threshold() {
	static const time_nsec_t threshold = []() {
		auto value = std::getenv("OPENAGE_STALL_PROFILE_MS");
		if (value == nullptr) {
			return time_nsec_t{0};
		}

		std::string_view text{value};
		time_nsec_t milliseconds = 0;
		auto result = std::from_chars(text.data(), text.data() + text.size(), milliseconds);
		if (result.ec != std::errc{} or result.ptr != text.data() + text.size()
		    or milliseconds == 0) {
			log::log(WARN << "Ignoring invalid OPENAGE_STALL_PROFILE_MS value '"
			              << text << "'; expected a positive integer");
			return time_nsec_t{0};
		}

		log::log(INFO << "Stall profiling enabled with a " << milliseconds
		              << " ms threshold");
		return milliseconds * NANOSECONDS_PER_MILLISECOND;
	}();
	return threshold;
}

} // namespace

StallProfiler::StallProfiler(const char *label) :
	label{label},
	threshold{get_stall_threshold()},
	start_time{0},
	running{this->threshold > 0} {
	if (this->running) {
		this->start_time = timing::get_monotonic_time();
	}
}

StallProfiler::~StallProfiler() {
	this->finish();
}

void StallProfiler::finish() {
	if (not this->running) {
		return;
	}

	this->running = false;
	auto duration = timing::get_monotonic_time() - this->start_time;
	if (duration >= this->threshold) {
		log::log(WARN << "Stall profile: " << this->label << " took "
		              << duration / NANOSECONDS_PER_MILLISECOND << " ms");
	}
}

} // namespace openage::util
