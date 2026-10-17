// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay.h"

#include <exception>
#include <thread>

#include "testing/testing.h"

namespace openage::network::tests {

namespace {

RelayToken test_token(uint8_t seed) {
	RelayToken token{};
	for (size_t i = 0; i < token.size(); ++i) {
		token[i] = static_cast<uint8_t>(seed + i);
	}
	return token;
}

RelayPairCredentials test_credentials() {
	return {
		17,
		2,
		test_token(1),
		test_token(101),
	};
}

RelayConnect test_connect(RelayRole role) {
	auto credentials = test_credentials();
	return {
		credentials.match_id,
		credentials.player_id,
		role,
		role == RelayRole::HOST ? credentials.host_token : credentials.guest_token,
	};
}

} // namespace

void relay() {
	auto connect = test_connect(RelayRole::GUEST);
	auto decoded = decode_relay_connect(encode_relay_connect(connect));
	TESTEQUALS(decoded.match_id, connect.match_id);
	TESTEQUALS(decoded.player_id, connect.player_id);
	decoded.role == RelayRole::GUEST or TESTFAIL;
	decoded.token == connect.token or TESTFAIL;

	auto invalid = connect;
	invalid.match_id = 0;
	TESTTHROWS(encode_relay_connect(invalid));
	invalid = connect;
	invalid.role = static_cast<RelayRole>(99);
	TESTTHROWS(encode_relay_connect(invalid));
	auto truncated = encode_relay_connect(connect);
	truncated.pop_back();
	TESTTHROWS(decode_relay_connect(truncated));

	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr relay_error{};
	std::jthread relay_server{[&]() {
		try {
			forward_relay_pair(accept_relay_pair(listener, test_credentials()));
		}
		catch (...) {
			relay_error = std::current_exception();
		}
	}};

	auto unauthorized = test_connect(RelayRole::GUEST);
	unauthorized.token = test_token(201);
	TESTTHROWS(connect_relay("127.0.0.1", listener.get_port(), unauthorized));

	std::exception_ptr host_error{};
	std::jthread host{[&]() {
		try {
			auto connection = connect_relay(
				"127.0.0.1", listener.get_port(), test_connect(RelayRole::HOST));
			Frame ping{};
			ping.message_type = MessageType::PING;
			ping.session_id = 42;
			ping.sequence = 1;
			ping.payload = {1, 2, 3};
			connection.send_frame(ping);

			auto response = connection.receive_frame();
			response.has_value() or TESTFAIL;
			response->message_type == MessageType::PONG or TESTFAIL;
			TESTEQUALS(response->session_id, uint64_t{42});
			TESTEQUALS(response->sequence, uint64_t{1});
			response->payload == ping.payload or TESTFAIL;
		}
		catch (...) {
			host_error = std::current_exception();
		}
	}};

	auto guest = connect_relay(
		"127.0.0.1", listener.get_port(), test_connect(RelayRole::GUEST));
	auto ping = guest.receive_frame();
	ping.has_value() or TESTFAIL;
	ping->message_type == MessageType::PING or TESTFAIL;
	Frame pong = *ping;
	pong.message_type = MessageType::PONG;
	guest.send_frame(pong);
	guest.close();

	host.join();
	relay_server.join();
	if (host_error) {
		std::rethrow_exception(host_error);
	}
	if (relay_error) {
		std::rethrow_exception(relay_error);
	}
}

} // namespace openage::network::tests
