// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby_relay_client.h"

#include <chrono>
#include <exception>
#include <thread>

#include "network/relay.h"
#include "network/relay_service.h"
#include "network/replication.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

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
	bad.name = "Test";
	bad.room_name = "Test Room";
	bad.hosting = true;
	bad.token_hex = "foo";
	TESTTHROWS(invalid.connect(bad));
	TESTEQUALS(invalid.status().phase, std::string{"menu"});

	RelayRoomDirectory directory{4};
	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr relay_error{};
	std::jthread relay{[&]() {
		try {
			serve_relay_directory(listener, directory, 8);
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
	conf.name = "HostPlayer";
	conf.room_name = "Automated Test";
	conf.hosting = true;
	host.connect(conf);
	TESTEQUALS(wait_for(host, "waiting"), true);

	conf.name = "GuestPlayer";
	guest.refresh(conf);
	TESTEQUALS(wait_for(guest, "menu"), true);
	TESTEQUALS(guest.status().rooms.find("Automated Test") != std::string::npos, true);
	conf.room_id = guest.status().suggested_room_id;
	TESTEQUALS(conf.room_id != 0, true);
	conf.name = "GuestPlayer";
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

	guest.close();
	TESTEQUALS(wait_for(host, "error"), true);
	listener.close();
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
	host.close();
	TESTEQUALS(host.status().phase, std::string{"menu"});
}

} // namespace openage::network::tests
