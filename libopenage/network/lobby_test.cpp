// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "network/protocol.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

uint64_t zero_draw(uint64_t bound) {
	bound > 0 or TESTFAIL;
	return 0;
}

uint64_t hunter_of(const StartMatch &started) {
	for (const auto &p : started.assignments) {
		if (p.role == PlayerRole::HUNTER) {
			return p.participant_id;
		}
	}
	TESTFAIL;
	return 0;
}

} // namespace

void lobby() {
	TESTTHROWS(HostLobby(0, "Host"));
	TESTTHROWS(HostLobby(11, ""));
	TESTTHROWS(HostLobby(11, std::string(MAX_PARTICIPANT_NAME_SIZE + 1, 'x')));
	TESTTHROWS(HostLobby(11, "\t  "));
	TESTTHROWS(HostLobby(11, "bad\nname"));

	HostLobby host{11, "Host"};
	TESTEQUALS(host.started(), false);
	TESTEQUALS(host.size(), size_t{1});
	TESTEQUALS(host.snapshot().revision, uint64_t{1});
	TESTEQUALS(host.snapshot().host_participant_id, uint64_t{11});
	TESTEQUALS(host.snapshot().participants.front().is_host, true);
	TESTTHROWS(host.start(zero_draw));
	TESTEQUALS(host.started(), false);
	TESTTHROWS(host.join(0, "Bad"));
	TESTTHROWS(host.join(11, "Duplicate"));
	TESTTHROWS(host.join(22, " "));
	host.join(22, "Guest");
	TESTEQUALS(host.snapshot().revision, uint64_t{2});
	TESTTHROWS(host.join(22, "Duplicate"));
	TESTTHROWS(host.leave(11));
	TESTTHROWS(host.leave(99));

	const auto snapshot = host.snapshot();
	auto encoded_snapshot = encode_participant_snapshot(snapshot);
	auto decoded_snapshot = decode_participant_snapshot(encoded_snapshot);
	TESTEQUALS(decoded_snapshot.revision, uint64_t{2});
	TESTEQUALS(decoded_snapshot.participants.size(), size_t{2});
	TESTEQUALS(decoded_snapshot.participants.at(1).participant_id, uint64_t{22});
	TESTEQUALS(decoded_snapshot.participants.at(1).name, std::string{"Guest"});
	TESTEQUALS(decoded_snapshot.participants.at(1).is_host, false);
	auto truncated = encoded_snapshot;
	truncated.pop_back();
	TESTTHROWS(decode_participant_snapshot(truncated));
	auto trailing = encoded_snapshot;
	trailing.push_back(0);
	TESTTHROWS(decode_participant_snapshot(trailing));
	auto bad_flags = encoded_snapshot;
	bad_flags[29] = 2; // host participant flags, big-endian
	TESTTHROWS(decode_participant_snapshot(bad_flags));
	auto bad_size = encoded_snapshot;
	bad_size[18] = 0;
	bad_size[19] = 9;
	TESTTHROWS(decode_participant_snapshot(bad_size));
	auto bad_snapshot = snapshot;
	bad_snapshot.participants[1].participant_id = 11;
	TESTTHROWS(encode_participant_snapshot(bad_snapshot));
	bad_snapshot = snapshot;
	bad_snapshot.participants[1].is_host = true;
	TESTTHROWS(encode_participant_snapshot(bad_snapshot));

	// 2-player probabilities: host occupies one of three equiprobable
	// tickets, guest occupies the other two. No extra random ticket is
	// needed to assign the one remaining Builder.
	for (uint64_t ticket = 0; ticket < 3; ++ticket) {
		HostLobby two{11, "Host"};
		two.join(22, "Guest");
		size_t calls = 0;
		auto started = two.start([&](uint64_t bound) {
			TESTEQUALS(bound, uint64_t{3});
			++calls;
			return ticket;
		});
		TESTEQUALS(calls, size_t{1});
		TESTEQUALS(started.lobby_revision, uint64_t{3});
		TESTEQUALS(started.assignments.size(), size_t{2});
		TESTEQUALS(hunter_of(started), uint64_t{ticket == 0 ? 11 : 22});
		TESTEQUALS(started.assignments[ticket == 0 ? 0 : 1].player_id,
		           gamestate::player_id_t{1});
		TESTEQUALS(two.started(), true);
		TESTTHROWS(two.join(33, "Late joiner"));
		TESTTHROWS(two.leave(22));
		TESTTHROWS(two.start(zero_draw));
	}

	HostLobby eight{100, "Host"};
	for (uint64_t i = 1; i <= 7; ++i) {
		eight.join(100 + i, "Guest " + std::to_string(i));
	}
	TESTEQUALS(eight.size(), size_t{8});
	TESTTHROWS(eight.join(108, "Overflow"));
	TESTEQUALS(eight.snapshot().revision, uint64_t{8});
	eight.leave(104);
	TESTEQUALS(eight.size(), size_t{7});
	TESTEQUALS(eight.snapshot().revision, uint64_t{9});
	eight.join(108, "Replacement");
	TESTEQUALS(eight.size(), size_t{8});

	// Enumerate all 15 Hunter tickets at N=8. Host occurs once, each guest
	// exactly twice; every result has all eight distinct player slots.
	std::array<uint32_t, 8> hunter_hits{};
	for (uint64_t ticket = 0; ticket < 15; ++ticket) {
		HostLobby many{100, "Host"};
		for (uint64_t i = 1; i <= 7; ++i) {
			many.join(100 + i, "Guest " + std::to_string(i));
		}
		size_t calls = 0;
		auto result = many.start([&](uint64_t bound) {
			++calls;
			if (calls == 1) {
				TESTEQUALS(bound, uint64_t{15});
				return ticket;
			}
			// Deterministic Fisher-Yates selection; always in range.
			return bound - 1;
		});
		TESTEQUALS(calls, size_t{7});
		std::array<bool, 9> used{};
		for (const auto &assignment : result.assignments) {
			const auto slot = static_cast<size_t>(assignment.player_id);
			TESTEQUALS(slot >= 1 and slot <= 8, true);
			TESTEQUALS(used[slot], false);
			used[slot] = true;
		}
		TESTEQUALS(std::all_of(used.begin() + 1, used.end(),
		                      [](bool taken) { return taken; }), true);
		const auto hunter = hunter_of(result);
		TESTEQUALS(hunter >= 100 and hunter <= 107, true);
		++hunter_hits[hunter - 100];
	}
	TESTEQUALS(hunter_hits[0], uint32_t{1});
	for (size_t i = 1; i < hunter_hits.size(); ++i) {
		TESTEQUALS(hunter_hits[i], uint32_t{2});
	}

	// Bad or throwing entropy does not latch started or change membership.
	HostLobby failed{11, "Host"};
	failed.join(22, "Guest");
	TESTTHROWS(failed.start({}));
	TESTTHROWS(failed.start([](uint64_t n) { return n; }));
	TESTTHROWS(failed.start([](uint64_t) -> uint64_t {
		throw std::runtime_error("random source failure");
	}));
	TESTEQUALS(failed.started(), false);
	TESTEQUALS(failed.snapshot().revision, uint64_t{2});
	auto successful = failed.start(zero_draw);
	auto encoded_start = encode_start_match(successful);
	auto decoded_start = decode_start_match(encoded_start);
	TESTEQUALS(decoded_start.host_participant_id, uint64_t{11});
	TESTEQUALS(decoded_start.lobby_revision, uint64_t{3});
	TESTEQUALS(decoded_start.assignments.size(), size_t{2});
	TESTEQUALS(decoded_start.assignments.at(0).participant_id, uint64_t{11});
	TESTEQUALS(decoded_start.assignments.at(0).role == PlayerRole::HUNTER, true);
	TESTEQUALS(decoded_start.assignments.at(1).player_id,
	           gamestate::player_id_t{2});

	auto bad_start = successful;
	bad_start.assignments.at(1).player_id = 1;
	TESTTHROWS(encode_start_match(bad_start));
	bad_start = successful;
	bad_start.assignments.at(1).role = PlayerRole::HUNTER;
	TESTTHROWS(encode_start_match(bad_start));
	bad_start = successful;
	bad_start.assignments.pop_back();
	TESTTHROWS(encode_start_match(bad_start));
	auto truncated_start = encoded_start;
	truncated_start.pop_back();
	TESTTHROWS(decode_start_match(truncated_start));
	auto trailing_start = encoded_start;
	trailing_start.push_back(0);
	TESTTHROWS(decode_start_match(trailing_start));
	auto bad_role = encoded_start;
	bad_role[37] = 9; // role for first assignment
	TESTTHROWS(decode_start_match(bad_role));
	auto extra_flags = encoded_start;
	extra_flags[19] = 1; // reserved header flags
	TESTTHROWS(decode_start_match(extra_flags));

	Frame announcement{};
	announcement.message_type = MessageType::START_MATCH;
	announcement.payload = encoded_start;
	auto roundtrip = decode_frame(encode_frame(announcement));
	TESTEQUALS(roundtrip.message_type == MessageType::START_MATCH, true);
	TESTEQUALS(decode_start_match(roundtrip.payload).assignments.size(), size_t{2});
}

} // namespace openage::network::tests
