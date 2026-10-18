// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby_relay_client.h"

#include <chrono>
#include <exception>
#include <thread>

#include "network/relay.h"
#include "network/replication.h"
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
	TESTEQUALS(host.status().roles, guest.status().roles);
	TESTEQUALS(host.status().roles.find("HostPlayer") != std::string::npos, true);
	TESTEQUALS(host.status().roles.find("GuestPlayer") != std::string::npos, true);
	TESTEQUALS(host.status().roles.find("HUNTER") != std::string::npos, true);
	TESTEQUALS(host.status().roles.find("BUILDER") != std::string::npos, true);
	TESTEQUALS(host.status().local_role != guest.status().local_role, true);
	TESTEQUALS(host.status().local_role == "HUNTER" or host.status().local_role == "BUILDER", true);
	TESTEQUALS(guest.status().local_role == "HUNTER" or guest.status().local_role == "BUILDER", true);
	// The timer is monotonic; delayed CI scheduling may already have passed
	// the five-second interval. Exact boundary checks live in lobby_test.
	TESTEQUALS(host.status().role_seconds_remaining <= 5, true);
	TESTEQUALS(guest.status().role_seconds_remaining <= 5, true);
	TESTEQUALS(host.status().local_player_id != guest.status().local_player_id, true);
	TESTEQUALS(host.status().remote_player_id, guest.status().local_player_id);
	TESTEQUALS(guest.status().remote_player_id, host.status().local_player_id);
	TESTEQUALS(host.status().local_player_id + guest.status().local_player_id,
	           gamestate::player_id_t{3});
	TESTEQUALS(guest.get_replication() == nullptr, true);

	WorldState initial{};
	initial.map_size = 128;
	initial.world_revision = 1;
	initial.local_player_id = guest.status().local_player_id;
	initial.local_kill_count = 7;
	initial.players = {
		{1, PlayerRole::HUNTER, true},
		{2, PlayerRole::BUILDER, true},
	};
	auto wrong_recipient = initial;
	wrong_recipient.local_player_id = host.status().local_player_id;
	TESTTHROWS(host.provide_host_snapshot(wrong_recipient));
	auto wrong_base = initial;
	wrong_base.base_revision = 3;
	TESTTHROWS(host.provide_host_snapshot(wrong_base));

	host.provide_host_snapshot(initial);
	TESTTHROWS(host.provide_host_snapshot(initial));
	TESTEQUALS(wait_for(guest, "bootstrap"), true);
	TESTEQUALS(wait_for(host, "bootstrap"), true);
	auto replication = guest.get_replication();
	TESTEQUALS(replication != nullptr, true);
	TESTEQUALS(replication->get_local_player_id(), initial.local_player_id);
	TESTEQUALS(replication->get_revision(), uint64_t{1});
	TESTEQUALS(replication->get_local_kills().value(), uint64_t{7});
	TESTEQUALS(replication->get_world_state()->players.size(), size_t{2});
	TESTEQUALS(host.status().snapshot_revision, uint64_t{1});
	TESTEQUALS(guest.status().snapshot_revision, uint64_t{1});
	TESTEQUALS(host.status().roles, guest.status().roles);
	TESTEQUALS(host.status().local_role != guest.status().local_role, true);

	guest.close();
	TESTEQUALS(wait_for(host, "error"), true);
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
	host.close();
	TESTEQUALS(host.status().phase, std::string{"menu"});
}

} // namespace openage::network::tests
