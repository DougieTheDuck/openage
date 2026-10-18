// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_service.h"

#include <chrono>
#include <exception>
#include <thread>

#include "network/relay.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

RelayToken service_test_token(uint8_t seed) {
	RelayToken token{};
	for (size_t i = 0; i < token.size(); ++i) {
		token[i] = static_cast<uint8_t>(seed + i);
	}
	return token;
}

} // namespace

void relay_service() {
	auto host_token = service_test_token(1);
	auto guest_token = service_test_token(101);
	RelayRoomDirectory directory{1, {host_token, guest_token}};
	TESTEQUALS(directory.requires_token(), true);
	TESTEQUALS(directory.room_count(), size_t{0});

	auto listener = TcpListener::listen("127.0.0.1", 0);
	auto port = listener.get_port();
	std::exception_ptr service_error{};
	std::jthread service{[&]() {
		try {
			serve_relay_directory(listener, directory, 8);
		}
		catch (...) {
			service_error = std::current_exception();
		}
	}};

	RelayToken unauthorized{};
	TESTTHROWS(list_relay_rooms("127.0.0.1", port, unauthorized));
	auto hosted = create_relay_room(
		"127.0.0.1", port, host_token, "Test Room", "Host Player");
	TESTEQUALS(hosted.room_id != 0, true);
	TESTEQUALS(directory.room_count(), size_t{1});
	TESTTHROWS(create_relay_room(
		"127.0.0.1", port, guest_token, "Overflow Room", "Other Host"));
	TESTEQUALS(directory.room_count(), size_t{1});

	auto rooms = list_relay_rooms("127.0.0.1", port, guest_token);
	TESTEQUALS(rooms.size(), size_t{1});
	TESTEQUALS(rooms[0].room_id, hosted.room_id);
	TESTEQUALS(rooms[0].room_name, std::string{"Test Room"});
	TESTEQUALS(rooms[0].host_name, std::string{"Host Player"});
	TESTTHROWS(join_relay_room("127.0.0.1", port, host_token, hosted.room_id));
	TESTEQUALS(directory.room_count(), size_t{1});

	auto guest = join_relay_room(
		"127.0.0.1", port, guest_token, hosted.room_id);
	await_relay_room(hosted.connection, hosted.room_id);
	TESTEQUALS(directory.room_count(), size_t{0});
	TESTEQUALS(list_relay_rooms("127.0.0.1", port, guest_token).empty(), true);

	Frame frame{};
	frame.message_type = MessageType::PING;
	frame.session_id = 201;
	frame.sequence = 1;
	frame.payload = {1, 2, 3};
	hosted.connection.send_frame(frame);
	auto forwarded = guest.receive_frame();
	forwarded.has_value() or TESTFAIL;
	TESTEQUALS(forwarded->session_id, frame.session_id);
	TESTEQUALS(forwarded->payload == frame.payload, true);

	// A closed host must not leave an advertised, joinable phantom room.
	// The relay's old send-only heartbeat could succeed after peer close.
	auto abandoned = create_relay_room(
		"127.0.0.1", port, host_token, "Abandoned Room", "Former Host");
	TESTEQUALS(directory.room_count(), size_t{1});
	abandoned.connection.close();
	bool expired = false;
	for (size_t attempt = 0; attempt < 50; ++attempt) {
		if (list_relay_rooms("127.0.0.1", port, guest_token).empty()) {
			expired = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(expired, true);
	TESTEQUALS(directory.room_count(), size_t{0});
	TESTTHROWS(join_relay_room("127.0.0.1", port, guest_token, abandoned.room_id));

	listener.close();
	service.join();
	if (service_error) {
		std::rethrow_exception(service_error);
	}
	hosted.connection.close();
	guest.close();

	RelayRoomDirectory public_directory{1};
	TESTEQUALS(public_directory.requires_token(), false);
}

} // namespace openage::network::tests
