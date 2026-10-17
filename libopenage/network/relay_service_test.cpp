// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_service.h"

#include <exception>
#include <optional>
#include <thread>
#include <utility>

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

RelayPairCredentials service_test_credentials(gamestate::player_id_t player_id,
                                              uint8_t seed) {
	return {
		91,
		player_id,
		service_test_token(seed),
		service_test_token(static_cast<uint8_t>(seed + 40)),
	};
}

RelayConnect service_test_connect(const RelayPairCredentials &credentials,
                                  RelayRole role) {
	return {
		credentials.match_id,
		credentials.player_id,
		role,
		role == RelayRole::HOST ? credentials.host_token : credentials.guest_token,
	};
}

} // namespace

void relay_service() {
	auto first_credentials = service_test_credentials(2, 1);
	auto second_credentials = service_test_credentials(3, 101);
	RelayPairRegistry registry{2};
	registry.provision(first_credentials);
	registry.provision(second_credentials);
	auto listener = TcpListener::listen("127.0.0.1", 0);
	auto port = listener.get_port();

	std::exception_ptr service_error{};
	std::jthread service{[&]() {
		try {
			serve_relay_pairs(listener, registry, 2);
		}
		catch (...) {
			service_error = std::current_exception();
		}
	}};
	auto stalled_admission = TcpConnection::connect("127.0.0.1", port);

	std::optional<TcpConnection> first_host{};
	std::optional<TcpConnection> first_guest{};
	std::optional<TcpConnection> second_host{};
	std::optional<TcpConnection> second_guest{};
	std::exception_ptr connection_errors[4]{};
	std::jthread connections[4]{
		std::jthread{[&]() {
			try {
				first_host = connect_relay(
					"127.0.0.1", port, service_test_connect(first_credentials, RelayRole::HOST));
			}
			catch (...) {
				connection_errors[0] = std::current_exception();
			}
		}},
		std::jthread{[&]() {
			try {
				second_guest = connect_relay(
					"127.0.0.1", port, service_test_connect(second_credentials, RelayRole::GUEST));
			}
			catch (...) {
				connection_errors[1] = std::current_exception();
			}
		}},
		std::jthread{[&]() {
			try {
				first_guest = connect_relay(
					"127.0.0.1", port, service_test_connect(first_credentials, RelayRole::GUEST));
			}
			catch (...) {
				connection_errors[2] = std::current_exception();
			}
		}},
		std::jthread{[&]() {
			try {
				second_host = connect_relay(
					"127.0.0.1", port, service_test_connect(second_credentials, RelayRole::HOST));
			}
			catch (...) {
				connection_errors[3] = std::current_exception();
			}
		}},
	};
	for (auto &connection : connections) {
		connection.join();
	}
	for (const auto &error : connection_errors) {
		if (error) {
			std::rethrow_exception(error);
		}
	}
	first_host.has_value() or TESTFAIL;
	first_guest.has_value() or TESTFAIL;
	second_host.has_value() or TESTFAIL;
	second_guest.has_value() or TESTFAIL;
	TESTEQUALS(stalled_admission.receive_frame().has_value(), false);

	Frame first_frame{};
	first_frame.message_type = MessageType::PING;
	first_frame.session_id = 201;
	first_frame.sequence = 1;
	first_frame.payload = {1};
	first_host->send_frame(first_frame);
	auto first_forwarded = first_guest->receive_frame();
	first_forwarded.has_value() or TESTFAIL;
	TESTEQUALS(first_forwarded->session_id, first_frame.session_id);

	Frame second_frame{};
	second_frame.message_type = MessageType::PING;
	second_frame.session_id = 202;
	second_frame.sequence = 1;
	second_frame.payload = {2};
	second_guest->send_frame(second_frame);
	auto second_forwarded = second_host->receive_frame();
	second_forwarded.has_value() or TESTFAIL;
	TESTEQUALS(second_forwarded->session_id, second_frame.session_id);

	first_host->close();
	first_guest->close();
	second_host->close();
	second_guest->close();
	service.join();
	if (service_error) {
		std::rethrow_exception(service_error);
	}
}

} // namespace openage::network::tests
