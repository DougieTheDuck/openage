// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include "timing.h"


namespace openage::util {

/**
 * Logs wall-clock time for an operation when it exceeds the configured stall
 * threshold.
 *
 * Profiling is enabled by setting OPENAGE_STALL_PROFILE_MS to a positive
 * integer number of milliseconds. Disabled profiling only performs one cached
 * threshold check per instance.
 */
class StallProfiler {
public:
	/**
	 * Start measuring an operation.
	 *
	 * @param label Stable operation name included in the diagnostic log.
	 */
	explicit StallProfiler(const char *label);

	~StallProfiler();

	StallProfiler(const StallProfiler &) = delete;
	StallProfiler &operator=(const StallProfiler &) = delete;
	StallProfiler(StallProfiler &&) = delete;
	StallProfiler &operator=(StallProfiler &&) = delete;

	/**
	 * Finish measuring and emit the diagnostic if the threshold was exceeded.
	 *
	 * Calling this more than once has no effect.
	 */
	void finish();

private:
	const char *label;
	time_nsec_t threshold;
	time_nsec_t start_time;
	bool running;
};

} // namespace openage::util
