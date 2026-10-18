// Copyright 2026 the openage authors. See copying.md for legal info.

#include "clock.h"

#include "testing/testing.h"

namespace openage::time::tests {

void reset_match_clock() {
	Clock clock;
	clock.start();
	clock.pause();
	clock.reset_paused();
	TESTEQUALS(clock.get_state() == ClockState::PAUSED, true);
	TESTEQUALS(clock.get_time(), time::time_t::from_int(0));
	TESTEQUALS(clock.get_real_time(), time::time_t::from_int(0));

	clock.resume();
	TESTEQUALS(clock.get_state() == ClockState::RUNNING, true);
	TESTTHROWS(clock.reset_paused());

	clock.pause();
	clock.reset_paused();
	TESTEQUALS(clock.get_time(), time::time_t::from_int(0));
}

} // namespace openage::time::tests
