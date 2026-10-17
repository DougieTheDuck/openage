// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby_relay_client.h"

#include <chrono>
#include <exception>
#include <thread>

#include "network/relay.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

std::string token_hex(const RelayToken &token) {
	constexpr char alphabet[] = "0123456789abcdef";
	std::string result;
	for (const auto byte : token) {
		result.push_back(alphabet[byte >> 4]);
		result.push_back(alphabet[byte & 15]);
	}
	return result;
}

bool wait_for(RelayLobbyClient &client, const std::string &phase) {
	for (size_t i = 0; i < 300; ++i) {
		const auto state = client.status();
		if (state.phase == phase) return true;
		if (state.phase == "error") return false;
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	return false;
}

} // namespace

void lobby_relay_client() {
	RelayLobbyClient invalid{};
	RelayLobbyConfig bad{};
	bad.match_id = 23;
	bad.pair_id = 2;
	bad.name = "Test";
	bad.token_hex = "foo";
	TESTTHROWS(invalid.connect(bad));
	TESTEQUALS(invalid.status().phase, std::string{"menu"});

	RelayPairCredentials credentials{};
	credentials.match_id = 23;
	credentials.player_id = 2;
	for (size_t i = 0; i < RELAY_TOKEN_SIZE; ++i) {
		credentials.host_token[i] = static_cast<uint8_t>(i + 1);
		credentials.guest_token[i] = static_cast<uint8_t>(i + 101);
	}

	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr relay_error{};
	std::jthread relay{[&]() {
		try {
			forward_relay_pair(accept_relay_pair(listener, credentials));
		}
		catch (...) {
			relay_error = std::current_exception();
		}
	}};

	RelayLobbyClient host{};
	RelayLobbyClient guest{};
	RelayLobbyConfig conf{};
	conf.address = "127.0.0.1";
	conf.port = listener.get_port();
	conf.match_id = credentials.match_id;
	conf.pair_id = credentials.player_id;
	conf.name = "HostPlayer";
	conf.token_hex = token_hex(credentials.host_token);
	conf.hosting = true;
	host.connect(conf);
	conf.name = "GuestPlayer";
	conf.token_hex = token_hex(credentials.guest_token);
	conf.hosting = false;
	guest.connect(conf);

	TESTEQUALS(wait_for(host, "lobby"), true);
	TESTEQUALS(wait_for(guest, "lobby"), true);
	TESTEQUALS(host.status().can_start, true);
	TESTEQUALS(guest.status().can_start, false);
	TESTEQUALS(host.status().participants.find("GuestPlayer") != std::string::npos, true);
	TESTEQUALS(guest.status().participants.find("HostPlayer") != std::string::npos, true);
	// A guest cannot start the match by setting its own flag.
	guest.request_start();
	TESTEQUALS(guest.status().phase, std::string{"lobby"});
	host.request_start();
	TESTEQUALS(wait_for(host, "assigned"), true);
	TESTEQUALS(wait_for(guest, "assigned"), true);
	TESTEQUALS(host.status().participants, guest.status().participants);
	TESTEQUALS(host.status().participants.find("Hunter") != std::string::npos, true);
	TESTEQUALS(host.status().participants.find("Builder") != std::string::npos, true);
	host.close();
	guest.close();
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
	TESTEQUALS(host.status().phase, std::string{"menu"});
}

} // namespace openage::network::tests
