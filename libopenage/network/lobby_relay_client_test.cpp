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

	// The match-ending rule belongs to the assigned gameplay role,
	// not to a guest's connection position in the lobby.
	RelayLobbyStatus roles{};
	roles.phase = "bootstrap";
	roles.connected = true;
	roles.local_role = "HUNTER";
	roles.local_player_id = 1;
	roles.remote_player_id = 2;
	TESTEQUALS(host_can_continue_after_guest_loss(roles), true);
	roles.phase = "streaming";
	TESTEQUALS(host_can_continue_after_guest_loss(roles), true);
	roles.phase = "playing";
	TESTEQUALS(host_can_continue_after_guest_loss(roles), true);
	roles.phase = "assigned";
	TESTEQUALS(host_can_continue_after_guest_loss(roles), false);
	roles.phase = "playing";
	roles.local_role = "BUILDER";
	roles.local_player_id = 2;
	roles.remote_player_id = 1;
	TESTEQUALS(host_can_continue_after_guest_loss(roles), false);
	roles.local_role = "HUNTER";
	roles.local_player_id = 1;
	roles.remote_player_id = 2;
	roles.connected = false;
	TESTEQUALS(host_can_continue_after_guest_loss(roles), false);

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
	// Public-room fields must not be displaced by the appended role fields.
	// The old positional publish() construction silently coupled their order.
	const auto room_listing = guest.status();
	TESTEQUALS(room_listing.rooms.find("Automated Test") != std::string::npos, true);
	TESTEQUALS(room_listing.suggested_room_id != 0, true);
	TESTEQUALS(room_listing.roles.empty(), true);
	TESTEQUALS(room_listing.local_role.empty(), true);
	TESTEQUALS(room_listing.role_seconds_remaining, uint8_t{0});
	conf.room_id = room_listing.suggested_room_id;
	TESTEQUALS(conf.room_id != 0, true);
	conf.name = "GuestPlayer";
	conf.hosting = false;
	guest.connect(conf);

	TESTEQUALS(wait_for(host, "lobby"), true);
	TESTEQUALS(wait_for(guest, "lobby"), true);
	TESTEQUALS(host.status().can_start, true);
	TESTEQUALS(guest.status().can_start, false);
	TESTEQUALS(host.status().rooms.empty(), true);
	TESTEQUALS(guest.status().roles.empty(), true);
	TESTEQUALS(host.status().roles.empty(), true);
	TESTEQUALS(host.status().participants.find("GuestPlayer") != std::string::npos, true);
	TESTEQUALS(guest.status().participants.find("HostPlayer") != std::string::npos, true);
	// A guest cannot start the match by setting its own flag.
	guest.request_start();
	TESTEQUALS(guest.status().phase, std::string{"lobby"});
	host.request_start();
	TESTEQUALS(wait_for(host, "assigned"), true);
	TESTEQUALS(wait_for(guest, "assigned"), true);
	TESTEQUALS(host.status().roles, guest.status().roles);
	TESTEQUALS(host.status().rooms.empty(), true);
	TESTEQUALS(host.status().suggested_room_id, uint64_t{0});
	TESTEQUALS(guest.status().rooms.empty(), true);
	TESTEQUALS(guest.status().suggested_room_id, uint64_t{0});
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

	// The same authenticated relay session must continue carrying host
	// changes after MATCH_READY; no second connection or new role draw.
	auto next = initial;
	next.world_revision = 2;
	next.local_kill_count = 8;
	auto wrong_update = next;
	wrong_update.local_player_id = host.status().local_player_id;
	TESTTHROWS(host.provide_host_update(wrong_update));
	host.provide_host_update(next);
	TESTTHROWS(host.provide_host_update(initial));
	TESTEQUALS(wait_for(host, "streaming"), true);
	TESTEQUALS(wait_for(guest, "streaming"), true);
	TESTEQUALS(guest.status().snapshot_revision, uint64_t{2});
	TESTEQUALS(replication->get_revision(), uint64_t{2});
	TESTEQUALS(replication->get_local_kills().value(), uint64_t{8});
	TESTEQUALS(guest.status().roles, host.status().roles);

	// Revision numbers can skip captured intermediate samples: the world
	// delta base is the last transmitted revision, not revision minus one.
	auto later = next;
	later.world_revision = 4;
	later.local_kill_count = 10;
	host.provide_host_update(later);
	bool got_later = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (guest.status().snapshot_revision == 4) {
			got_later = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(got_later, true);
	TESTEQUALS(replication->get_revision(), uint64_t{4});
	TESTEQUALS(replication->get_local_kills().value(), uint64_t{10});

	// The same session must recover a full recipient-specific world from
	// the host's most recently transmitted delta baseline.
	TESTEQUALS(guest.request_recovery_snapshot(), true);
	bool recovered = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (guest.status().snapshot_recoveries == 1) {
			recovered = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(recovered, true);
	TESTEQUALS(replication->get_revision(), uint64_t{4});
	TESTEQUALS(replication->get_local_kills().value(), uint64_t{10});

	// The role reveal must finish on both peers before human orders are sent.
	bool host_ready = false;
	bool guest_ready = false;
	for (size_t attempt = 0; attempt < 700; ++attempt) {
		host_ready = host.status().phase == "playing";
		guest_ready = guest.status().phase == "playing";
		if (host_ready and guest_ready) break;
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(host_ready, true);
	TESTEQUALS(guest_ready, true);
	// Even without guest commands, the accepted session remains alive via
	// validated PING/PONG frames rather than relying on TCP peer_connected().
	bool heartbeats_received = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (guest.status().heartbeat_acknowledgements >= 2) {
			heartbeats_received = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(heartbeats_received, true);
	TESTEQUALS(host.status().connected, true);
	TESTEQUALS(guest.status().connected, true);
	CommandIntent movement{};
	movement.kind = CommandKind::MOVE;
	movement.entity_ids = {42};
	movement.target = {17, 18, 0};
	TESTEQUALS(host.send_guest_command(movement), false);
	TESTEQUALS(guest.send_guest_command(movement), true);
	std::vector<AuthenticatedCommand> submitted{};
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		submitted = host.take_host_commands();
		if (not submitted.empty()) break;
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(submitted.size(), size_t{1});
	TESTEQUALS(submitted[0].identity.player_id, initial.local_player_id);
	TESTEQUALS(submitted[0].intent.kind == CommandKind::MOVE, true);
	TESTEQUALS(submitted[0].intent.entity_ids == movement.entity_ids, true);
	TESTEQUALS(submitted[0].intent.command_id, uint64_t{1});
	CommandResult answer{};
	answer.command_id = submitted[0].intent.command_id;
	answer.status = CommandStatus::REJECTED;
	answer.world_revision = 4;
	answer.reason = "unauthorized unit";
	const auto active_generation = host.get_session_generation();
	const auto source = submitted[0].identity;
	// A delayed callback cannot reply as a different player or a prior
	// session generation, even if its command ID is reused.
	TESTEQUALS(host.send_command_result(
		{source.session_id + 1, source.player_id}, active_generation, answer), false);
	TESTEQUALS(host.send_command_result(
		source, active_generation + 1, answer), false);
	TESTEQUALS(host.send_command_result(source, active_generation, answer), true);
	std::vector<CommandResult> decisions{};
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		decisions = guest.take_command_results();
		if (not decisions.empty()) break;
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(decisions.size(), size_t{1});
	TESTEQUALS(decisions[0].command_id, answer.command_id);
	TESTEQUALS(decisions[0].status == CommandStatus::REJECTED, true);
	TESTEQUALS(decisions[0].reason, answer.reason);
	TESTEQUALS(guest.status().pending_order_count, size_t{0});
	TESTEQUALS(guest.status().order_feedback.find("Move rejected") != std::string::npos, true);

	// If the guest is a Builder, the authoritative Hunter host continues;
	// if the guest is Hunter, losing them ends the match.
	const bool host_is_hunter = host.status().local_role == "HUNTER";
	guest.close();
	bool host_saw_departure = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (not host.status().connected) {
			host_saw_departure = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(host_saw_departure, true);
	TESTEQUALS(host.send_command_result(source, active_generation, answer), false);
	TESTEQUALS(host.status().can_start, false);
	if (host_is_hunter) {
		TESTEQUALS(host.status().phase, std::string{"playing"});
		TESTEQUALS(host.status().remote_player_id, gamestate::player_id_t{0});
		TESTEQUALS(host.status().message.find("Builder disconnected") != std::string::npos, true);
		TESTEQUALS(host.take_host_commands().empty(), true);
	}
	else {
		TESTEQUALS(host.status().phase, std::string{"error"});
	}
	TESTEQUALS(guest.send_guest_command(movement), false);
	host.close();
	TESTEQUALS(host.status().phase, std::string{"menu"});
	TESTEQUALS(host.send_command_result(source, active_generation, answer), false);
	TESTEQUALS(host.get_session_generation() != active_generation, true);

	// Test the opposite failure direction in a fresh relay room. Host loss
	// must invalidate the guest's authoritative state, including its score.
	RelayLobbyClient host2{};
	RelayLobbyClient guest2{};
	conf.hosting = true;
	conf.room_id = 0;
	conf.name = "HostAgain";
	host2.connect(conf);
	TESTEQUALS(wait_for(host2, "waiting"), true);
	guest2.refresh(conf);
	TESTEQUALS(wait_for(guest2, "menu"), true);
	conf.room_id = guest2.status().suggested_room_id;
	TESTEQUALS(conf.room_id != 0, true);
	conf.name = "GuestAgain";
	conf.hosting = false;
	guest2.connect(conf);
	TESTEQUALS(wait_for(host2, "lobby"), true);
	TESTEQUALS(wait_for(guest2, "lobby"), true);
	host2.request_start();
	TESTEQUALS(wait_for(host2, "assigned"), true);
	TESTEQUALS(wait_for(guest2, "assigned"), true);
	auto restart = initial;
	restart.local_player_id = guest2.status().local_player_id;
	host2.provide_host_snapshot(restart);
	TESTEQUALS(wait_for(guest2, "bootstrap"), true);
	TESTEQUALS(wait_for(host2, "bootstrap"), true);
	auto last_replication = guest2.get_replication();
	TESTEQUALS(last_replication != nullptr, true);
	TESTEQUALS(last_replication->is_host_connected(), true);
	host2.close();
	TESTEQUALS(wait_for(guest2, "error"), true);
	TESTEQUALS(guest2.status().connected, false);
	TESTEQUALS(last_replication->is_host_connected(), false);
	TESTEQUALS(last_replication->get_local_kills().has_value(), false);
	TESTEQUALS(guest2.send_guest_command(movement), false);
	guest2.close();
	listener.close();
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
}

} // namespace openage::network::tests
