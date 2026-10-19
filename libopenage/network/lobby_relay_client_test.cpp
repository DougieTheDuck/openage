// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby_relay_client.h"

#include <algorithm>
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

	// Special orders retain their payloads and command identities in the
	// authenticated relay, even with several in flight at once. The host
	// can decide independently; the guest's feedback follows each result.
	CommandIntent queued_tower{};
	queued_tower.kind = CommandKind::BUILD_TOWER;
	queued_tower.entity_ids = {42};
	queued_tower.target = {25, 26, 0};
	queued_tower.queue = true;
	CommandIntent focus{};
	focus.kind = CommandKind::FOCUS_ATTACK;
	focus.entity_ids = {55};
	focus.target_entity = gamestate::entity_id_t{77};
	focus.target = {30, 31, 0};
	CommandIntent prioritize{};
	prioritize.kind = CommandKind::PRIORITIZE_FOUNDATION;
	prioritize.entity_ids = {42};
	prioritize.target_entity = gamestate::entity_id_t{88};
	prioritize.target = {33, 34, 0};
	TESTEQUALS(guest.send_guest_command(queued_tower), true);
	TESTEQUALS(guest.send_guest_command(focus), true);
	TESTEQUALS(guest.send_guest_command(prioritize), true);
	TESTEQUALS(guest.status().pending_order_count, size_t{3});
	std::vector<AuthenticatedCommand> special{};
	for (size_t attempt = 0; attempt < 300 and special.size() < 3; ++attempt) {
		auto batch = host.take_host_commands();
		special.insert(special.end(), batch.begin(), batch.end());
		if (special.size() < 3) {
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
		}
	}
	TESTEQUALS(special.size(), size_t{3});
	TESTEQUALS(special[0].intent.command_id, uint64_t{2});
	TESTEQUALS(special[1].intent.command_id, uint64_t{3});
	TESTEQUALS(special[2].intent.command_id, uint64_t{4});
	TESTEQUALS(special[0].intent.kind == CommandKind::BUILD_TOWER, true);
	TESTEQUALS(special[0].intent.queue, true);
	TESTEQUALS(special[1].intent.kind == CommandKind::FOCUS_ATTACK, true);
	TESTEQUALS(special[1].intent.target_entity == focus.target_entity, true);
	TESTEQUALS(special[2].intent.kind == CommandKind::PRIORITIZE_FOUNDATION, true);
	TESTEQUALS(special[2].intent.target_entity == prioritize.target_entity, true);
	for (size_t i = 0; i < special.size(); ++i) {
		CommandResult decision{};
		decision.command_id = special[i].intent.command_id;
		decision.status = i == 2 ? CommandStatus::REJECTED : CommandStatus::ACCEPTED;
		decision.reason = i == 2 ? "foundation unavailable" : "";
		TESTEQUALS(host.send_command_result(source, active_generation, decision), true);
	}
	std::vector<CommandResult> special_results{};
	for (size_t attempt = 0; attempt < 300 and special_results.size() < 3; ++attempt) {
		auto batch = guest.take_command_results();
		special_results.insert(special_results.end(), batch.begin(), batch.end());
		if (special_results.size() < 3) {
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
		}
	}
	TESTEQUALS(special_results.size(), size_t{3});
	TESTEQUALS(guest.status().pending_order_count, size_t{0});
	TESTEQUALS(guest.status().order_feedback.find("Prioritize foundation rejected")
	           != std::string::npos, true);

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
	// Reuse the exact RelayLobbyClient to exercise generation changes across
	// F10-style close and reconnect, not merely a different object.
	RelayLobbyClient &host2 = host;
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
	TESTEQUALS(host2.get_session_generation() != active_generation, true);
	TESTEQUALS(host2.send_command_result(source, active_generation, answer), false);
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

	// End-to-end relay client integration with one authoritative host and
	// TWO live guests: shared role assignment, private snapshots, independent
	// command/decision channels, and continued play after Builder loss.
	RelayLobbyClient multiplayer_host{};
	RelayLobbyClient multi_guest_a{};
	RelayLobbyClient multi_guest_b{};
	RelayLobbyConfig multi{};
	multi.address = "127.0.0.1";
	multi.port = listener.get_port();
	multi.name = "Many Host";
	multi.room_name = "Three Human Game";
	multi.hosting = true;
	multi.player_capacity = 3;
	multiplayer_host.connect(multi);
	TESTEQUALS(wait_for(multiplayer_host, "waiting"), true);
	multi.hosting = false;
	multi.name = "Guest A";
	multi_guest_a.refresh(multi);
	TESTEQUALS(wait_for(multi_guest_a, "menu"), true);
	multi.room_id = multi_guest_a.status().suggested_room_id;
	TESTEQUALS(multi.room_id != uint64_t{0}, true);
	multi_guest_a.connect(multi);
	TESTEQUALS(wait_for(multi_guest_a, "lobby"), true);
	TESTEQUALS(wait_for(multiplayer_host, "lobby"), true);
	multi.name = "Guest B";
	multi_guest_b.connect(multi);
	TESTEQUALS(wait_for(multi_guest_b, "lobby"), true);
	bool both_in_lobby = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (multiplayer_host.status().participants.find("Guest B") != std::string::npos
		    and multi_guest_a.status().participants.find("Guest B") != std::string::npos) {
			both_in_lobby = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(both_in_lobby, true);
	multiplayer_host.request_start();
	TESTEQUALS(wait_for(multiplayer_host, "assigned"), true);
	TESTEQUALS(wait_for(multi_guest_a, "assigned"), true);
	TESTEQUALS(wait_for(multi_guest_b, "assigned"), true);
	auto multi_status = multiplayer_host.status();
	TESTEQUALS(multi_status.multi_host, true);
	TESTEQUALS(multi_status.match_player_count, size_t{3});
	TESTEQUALS(multi_status.remote_player_ids.size(), size_t{2});
	TESTEQUALS(multi_guest_a.status().roles, multi_guest_b.status().roles);
	TESTEQUALS(multi_guest_a.status().roles, multi_status.roles);
	TESTEQUALS(multi_guest_a.status().local_player_id
	           != multi_guest_b.status().local_player_id, true);
	const auto guest_a_id = multi_guest_a.status().local_player_id;
	const auto guest_b_id = multi_guest_b.status().local_player_id;
	for (auto id : multi_status.remote_player_ids) {
		WorldState world{};
		world.map_size = 128;
		world.world_revision = 1;
		world.local_player_id = id;
		world.local_kill_count = 10 + id;
		world.players = {
			{1, PlayerRole::HUNTER, true},
			{2, PlayerRole::BUILDER, true},
			{3, PlayerRole::BUILDER, true}};
		multiplayer_host.provide_host_snapshot(std::move(world));
	}
	TESTEQUALS(wait_for(multiplayer_host, "bootstrap"), true);
	TESTEQUALS(wait_for(multi_guest_a, "bootstrap"), true);
	TESTEQUALS(wait_for(multi_guest_b, "bootstrap"), true);
	TESTEQUALS(multi_guest_a.get_replication()->get_local_kills().value(),
	           uint64_t{10 + guest_a_id});
	TESTEQUALS(multi_guest_b.get_replication()->get_local_kills().value(),
	           uint64_t{10 + guest_b_id});
	WorldState update_a{};
	update_a.map_size = 128;
	update_a.world_revision = 2;
	update_a.local_player_id = guest_a_id;
	update_a.local_kill_count = 22;
	update_a.players = {
		{1, PlayerRole::HUNTER, true}, {2, PlayerRole::BUILDER, true},
		{3, PlayerRole::BUILDER, true}};
	auto update_b = update_a;
	update_b.local_player_id = guest_b_id;
	update_b.world_revision = 3;
	update_b.local_kill_count = 33;
	multiplayer_host.provide_host_update(update_a);
	multiplayer_host.provide_host_update(update_b);
	TESTEQUALS(wait_for(multi_guest_a, "streaming"), true);
	TESTEQUALS(wait_for(multi_guest_b, "streaming"), true);
	TESTEQUALS(multi_guest_a.get_replication()->get_local_kills().value(), uint64_t{22});
	TESTEQUALS(multi_guest_b.get_replication()->get_local_kills().value(), uint64_t{33});
	bool all_playing = false;
	for (size_t attempt = 0; attempt < 700; ++attempt) {
		if (multiplayer_host.status().phase == "playing"
		    and multi_guest_a.status().phase == "playing"
		    and multi_guest_b.status().phase == "playing") {
			all_playing = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(all_playing, true);
	CommandIntent guest_move{};
	guest_move.kind = CommandKind::MOVE;
	guest_move.entity_ids = {501};
	guest_move.target = {22, 23, 0};
	TESTEQUALS(multi_guest_a.send_guest_command(guest_move), true);
	guest_move.entity_ids = {502};
	TESTEQUALS(multi_guest_b.send_guest_command(guest_move), true);
	std::vector<AuthenticatedCommand> multi_commands{};
	for (size_t attempt = 0; attempt < 300 and multi_commands.size() < 2; ++attempt) {
		auto batch = multiplayer_host.take_host_commands();
		multi_commands.insert(multi_commands.end(), batch.begin(), batch.end());
		if (multi_commands.size() < 2)
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(multi_commands.size(), size_t{2});
	TESTEQUALS(multi_commands.at(0).identity.player_id
	           != multi_commands.at(1).identity.player_id, true);
	for (const auto &cmd : multi_commands) {
		CommandResult response{};
		response.command_id = cmd.intent.command_id;
		response.status = CommandStatus::ACCEPTED;
		response.world_revision = 3;
		TESTEQUALS(multiplayer_host.send_command_result(
			cmd.identity, multiplayer_host.get_session_generation(), response), true);
	}
	bool both_confirmed = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (multi_guest_a.status().pending_order_count == 0
		    and multi_guest_b.status().pending_order_count == 0) {
			both_confirmed = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(both_confirmed, true);
	// A Builder leaving must not end the Hunter's match or drop other guests.
	auto &departing = guest_a_id != 1 ? multi_guest_a : multi_guest_b;
	auto &survivor = guest_a_id != 1 ? multi_guest_b : multi_guest_a;
	const auto departed_id = departing.status().local_player_id;
	const auto survivor_id = survivor.status().local_player_id;
	departing.close();
	bool departed_recorded = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		const auto status = multiplayer_host.status();
		if (std::find(status.disconnected_players.begin(),
		              status.disconnected_players.end(), departed_id)
		    != status.disconnected_players.end()) {
			departed_recorded = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(departed_recorded, true);
	TESTEQUALS(multiplayer_host.status().phase, std::string{"playing"});
	TESTEQUALS(multiplayer_host.status().remote_player_ids.size(), size_t{1});
	bool survivor_notified = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		if (survivor.status().departure_notice.find("disconnected") != std::string::npos) {
			survivor_notified = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(survivor_notified, true);
	WorldState after_departure{};
	after_departure.map_size = 128;
	after_departure.world_revision = 4;
	after_departure.local_player_id = survivor_id;
	after_departure.players = {
		{1, PlayerRole::HUNTER, departed_id != 1},
		{2, PlayerRole::BUILDER, departed_id != 2},
		{3, PlayerRole::BUILDER, departed_id != 3}};
	multiplayer_host.provide_host_update(std::move(after_departure));
	bool updated_disconnected = false;
	for (size_t attempt = 0; attempt < 300; ++attempt) {
		auto current = survivor.get_replication()->get_world_state();
		if (current and current->world_revision == 4) {
			updated_disconnected = true;
			TESTEQUALS(current->players.at(departed_id - 1).connected, false);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(updated_disconnected, true);
	multiplayer_host.close();
	TESTEQUALS(wait_for(survivor, "error"), true);
	survivor.close();
	listener.close();
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
}

} // namespace openage::network::tests
