// Copyright 2026 the openage authors. See copying.md for legal info.

#include "guest_clock.h"

#include "renderer/stages/world/render_entity.h"
#include "testing/testing.h"
#include "time/clock.h"

namespace openage::presenter::tests {

void guest_response_latency() {
	const auto origin = std::chrono::steady_clock::time_point{};
	auto milliseconds = [](int64_t value) {
		return time::time_t::from_int(value) / 1000;
	};
	GuestClock clock{};
	renderer::world::RenderEntity visual{};
	clock.update(0, origin + std::chrono::milliseconds{20}, true);
	visual.update_sample(7, {0, 0, 0}, "unit.sprite", 0, std::nullopt);
	const int64_t input_time = 1013;
	const int64_t accepted_time = input_time + 20;
	int64_t next_sample = 50;
	std::optional<int64_t> first_response;
	for (int64_t frame = 20; frame <= 1500; frame += 5) {
		while (next_sample + 20 <= frame) {
			const auto received_at = origin + std::chrono::milliseconds{next_sample + 20};
			clock.update(milliseconds(next_sample), received_at, false);
			const auto displacement = coord::phys_t::from_int(
										  std::max(int64_t{0}, next_sample - accepted_time))
			                          * 3 / 1000;
			visual.update_sample(7, {displacement, 0, 0}, "unit.sprite", milliseconds(next_sample), clock.get_time(received_at));
			next_sample += 50;
		}
		const auto position = visual.get_position_at(clock.get_time(
			origin + std::chrono::milliseconds{frame}));
		if (position.ne > 0 and not first_response) {
			first_response = frame;
		}
	}
	first_response.has_value() or TESTFAIL;
	// Include a 20 ms command trip and 20 ms state delivery, without predicting movement.
	if (*first_response < accepted_time or *first_response - input_time > 150) {
		TESTFAILMSG("Guest response outside its latency budget: " << *first_response - input_time << " ms");
	}
}

void guest_clock_catchup() {
	const auto origin = std::chrono::steady_clock::time_point{};
	auto milliseconds = [](int64_t value) {
		return time::time_t::from_int(value) / 1000;
	};
	GuestClock clock{};
	clock.update(0, origin, true);
	for (int64_t sample = 50; sample <= 1000; sample += 50) {
		clock.update(milliseconds(sample), origin + std::chrono::milliseconds{sample}, false);
	}
	// Missing many updates must not leave controls a second behind while slowly catching up.
	TESTEQUALS(clock.update(milliseconds(2000), origin + std::chrono::seconds{2}, false).has_value(), false);
	TESTEQUALS(clock.get_time(origin + std::chrono::seconds{2}), milliseconds(2000));
	for (int64_t sample = 2050; sample <= 3000; sample += 50) {
		const auto now = origin + std::chrono::milliseconds{sample};
		clock.update(milliseconds(sample), now, false);
		if (milliseconds(sample) - clock.get_time(now) > milliseconds(100)) {
			TESTFAILMSG("Guest retained stale playback after a missed update burst");
		}
	}

	// A delayed first live capture must not be mistaken for a permanently slow update cadence.
	clock = GuestClock{};
	clock.update(0, origin, true);
	clock.update(milliseconds(500), origin + std::chrono::milliseconds{500}, false);
	for (int64_t sample = 550; sample <= 1500; sample += 50) {
		clock.update(milliseconds(sample), origin + std::chrono::milliseconds{sample}, false);
	}
	if (milliseconds(1500) - clock.get_time(origin + std::chrono::milliseconds{1500}) > milliseconds(100)) {
		TESTFAILMSG("Guest retained a large startup delay after the regular stream arrived");
	}
}

void guest_clock_jitter() {
	const auto origin = std::chrono::steady_clock::time_point{};
	auto milliseconds = [](int64_t value) {
		return time::time_t::from_int(value) / 1000;
	};
	// Include both replication cadences and host clock drift with uneven delivery.
	for (const int64_t host_interval : {250, 240, 50, 48}) {
		const int64_t wall_interval = host_interval >= 240 ? 250 : 50;
		// The faster stream needs room for delivery jitter comparable to an entire update.
		const auto maximum_buffer = milliseconds(host_interval) * (wall_interval == 250 ? 2 : 3);
		GuestClock clock{};
		clock.update(0, origin, true);
		time::time_t previous{0};
		int64_t next_sample = 1;
		for (int64_t frame = 0; frame < 12000; frame += 8) {
			while (true) {
				const int64_t jitter = next_sample % 4 == 2 ? 48 : (next_sample % 4 == 1 ? 16 : 0);
				const int64_t arrival = next_sample * wall_interval + jitter;
				if (arrival > frame) {
					break;
				}
				const auto now = origin + std::chrono::milliseconds{arrival};
				const auto before = clock.get_time(now);
				clock.update(milliseconds(next_sample * host_interval), now, false);
				TESTEQUALS(clock.get_time(now), before);
				// Extra revisions at the same host time must not consume or reset the buffer.
				clock.update(milliseconds(next_sample * host_interval), now, false);
				TESTEQUALS(clock.get_time(now), before);
				++next_sample;
			}
			const auto current = clock.get_time(origin + std::chrono::milliseconds{frame});
			if (frame >= 2000) {
				if (milliseconds((next_sample - 1) * host_interval) - current > maximum_buffer) {
					TESTFAILMSG("Guest playback exceeded its buffer budget");
				}
				const auto step = current - previous;
				if (step < milliseconds(8) * 9 / 10 or step > milliseconds(8) * 11 / 10) {
					TESTFAILMSG("Guest playback stalled or surged at " << frame
					                                                   << "ms: advanced " << step);
				}
			}
			previous = current;
		}
	}
}

void guest_interpolation() {
	const auto origin = std::chrono::steady_clock::time_point{};
	const auto second = time::time_t::from_int(1);
	GuestClock clock{};
	time::Clock simulation_clock{};
	simulation_clock.pause();
	TESTEQUALS(clock.get_time(origin), time::TIME_ZERO);
	TESTEQUALS(clock.update(0, origin, true).has_value(), false);
	TESTEQUALS(clock.get_time(origin + std::chrono::seconds{1}), time::TIME_ZERO);

	// Received endpoints bracket a moving render time despite a paused simulation.
	TESTEQUALS(clock.update(second, origin + std::chrono::seconds{1}, false).value(), time::TIME_ZERO);
	TESTEQUALS(clock.get_time(origin + std::chrono::milliseconds{1500}), time::TIME_ZERO);
	TESTEQUALS(clock.get_time(origin + std::chrono::seconds{2}), second / 2);
	TESTEQUALS(simulation_clock.get_time(), time::TIME_ZERO);

	renderer::world::RenderEntity visual{};
	visual.update_sample(7, {10, 20, 0}, "unit.sprite", 0, std::nullopt);
	visual.update_sample(7, {14, 24, 0}, "unit.sprite", second, time::TIME_ZERO);
	auto expect_position = [&](time::time_t time, coord::phys3 expected) {
		TESTEQUALS(visual.get_position_at(time) == expected.to_scene3(), true);
	};
	expect_position(0, {10, 20, 0});
	expect_position(clock.get_time(origin + std::chrono::seconds{2}), {12, 22, 0});
	expect_position(second, {14, 24, 0});
	expect_position(second * 10, {14, 24, 0});

	// An early packet preserves the current position and the unplayed turn.
	clock.update(second * 2, origin + std::chrono::seconds{2}, false);
	const auto playing = clock.get_time(origin + std::chrono::seconds{2});
	TESTEQUALS(playing, second / 2);
	visual.update_sample(7, {14, 16, 0}, "unit.sprite", second * 2, playing);
	expect_position(playing, {12, 22, 0});
	expect_position(second * 3 / 4, {13, 23, 0});
	expect_position(second * 3 / 2, {14, 20, 0});
	{
		auto lock = visual.get_read_lock();
		TESTEQUALS(visual.get_animation_paths().frame(second).first, time::TIME_ZERO);
	}

	// Equal timestamps replace the endpoint without a duplicate or a jump at playback time.
	visual.update_sample(7, {14, 18, 0}, "unit.sprite", second * 2, second * 3 / 2);
	expect_position(second * 3 / 2, {14, 20, 0});
	expect_position(second * 2, {14, 18, 0});
	visual.update_sample(7, {14, 18, 0}, "unit.sprite", second * 3, second * 2);
	expect_position(second * 5 / 2, {14, 18, 0});
	TESTTHROWS(visual.update_sample(7, {0, 0, 0}, "unit.sprite", second, second * 2));

	// Delayed traffic holds at known state, never extrapolates, and resumes from there.
	TESTEQUALS(clock.get_time(origin + std::chrono::seconds{10}), second * 2);
	clock.update(second * 3, origin + std::chrono::seconds{10}, false);
	TESTEQUALS(clock.get_time(origin + std::chrono::milliseconds{10500}), second * 2);
	TESTEQUALS(clock.get_time(origin + std::chrono::seconds{11}), second * 5 / 2);
	clock.update(second * 3, origin + std::chrono::seconds{11}, false);
	TESTEQUALS(clock.get_time(origin + std::chrono::milliseconds{11500}), second * 3);
	TESTTHROWS(clock.update(second, origin + std::chrono::milliseconds{11500}, false));

	// Recovery and new entities initialize at the received position without an old-world blend.
	TESTEQUALS(clock.update(second * 20, origin + std::chrono::seconds{12}, true).has_value(), false);
	visual.update_sample(7, {100, 200, 0}, "unit.sprite", second * 20, std::nullopt);
	expect_position(clock.get_time(origin + std::chrono::seconds{12}), {100, 200, 0});
	clock = GuestClock{};
	TESTEQUALS(clock.update(0, origin, false).has_value(), false);
	TESTEQUALS(clock.get_time(origin), time::TIME_ZERO);
}

} // namespace openage::presenter::tests
