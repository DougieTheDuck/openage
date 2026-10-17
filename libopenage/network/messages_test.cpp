// Copyright 2026 the openage authors. See copying.md for legal info.

#include "messages.h"

#include <string>
#include <vector>

#include "testing/testing.h"

namespace openage::network::tests {

void messages() {
	ClientHello hello{};
	hello.minimum_protocol_minor = 0;
	hello.participant_name = "Builder";
	hello.build_version = "fvd-test";
	for (size_t i = 0; i < hello.asset_digest.size(); ++i) {
		hello.asset_digest[i] = static_cast<uint8_t>(i);
	}
	auto encoded_hello = encode_client_hello(hello);
	auto decoded_hello = decode_client_hello(encoded_hello);
	TESTEQUALS(decoded_hello.minimum_protocol_minor, hello.minimum_protocol_minor);
	TESTEQUALS(decoded_hello.participant_name, hello.participant_name);
	TESTEQUALS(decoded_hello.build_version, hello.build_version);
	decoded_hello.asset_digest == hello.asset_digest or TESTFAIL;

	auto invalid_hello_flags = encoded_hello;
	invalid_hello_flags[3] = 1;
	TESTTHROWS(decode_client_hello(invalid_hello_flags));
	auto truncated_hello = encoded_hello;
	truncated_hello.pop_back();
	TESTTHROWS(decode_client_hello(truncated_hello));
	auto trailing_hello = encoded_hello;
	trailing_hello.push_back(0);
	TESTTHROWS(decode_client_hello(trailing_hello));
	ClientHello invalid_hello{};
	invalid_hello.build_version = "fvd-test";
	TESTTHROWS(encode_client_hello(invalid_hello));
	invalid_hello.participant_name = std::string(MAX_PARTICIPANT_NAME_SIZE + 1, 'x');
	TESTTHROWS(encode_client_hello(invalid_hello));

	ServerAccept accept{42, 3};
	auto decoded_accept = decode_server_accept(encode_server_accept(accept));
	TESTEQUALS(decoded_accept.session_id, accept.session_id);
	TESTEQUALS(decoded_accept.player_id, accept.player_id);
	TESTTHROWS(encode_server_accept({0, 3}));
	TESTTHROWS(encode_server_accept({42, 0}));
	auto trailing_accept = encode_server_accept(accept);
	trailing_accept.push_back(0);
	TESTTHROWS(decode_server_accept(trailing_accept));

	ServerReject reject{"gameplay asset manifest does not match host"};
	TESTEQUALS(decode_server_reject(encode_server_reject(reject)).reason,
	           reject.reason);
	TESTTHROWS(encode_server_reject({}));
	TESTTHROWS(encode_server_reject(
		{std::string(MAX_HANDSHAKE_REJECTION_SIZE + 1, 'x')}));

	CommandIntent intent{};
	intent.command_id = 0x0102030405060708;
	intent.kind = CommandKind::MOVE;
	intent.entity_ids = {0x1112131415161718, 0x2122232425262728};
	intent.target = {1.5, -2.25, 0};
	auto encoded_intent = encode_command_intent(intent);
	const std::vector<uint8_t> expected_intent{
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x00, 0x01,
		0x00, 0x00,
		0x00, 0x00, 0x00, 0x02,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xc0, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	encoded_intent == expected_intent or TESTFAIL;

	auto decoded_intent = decode_command_intent(encoded_intent);
	TESTEQUALS(decoded_intent.command_id, intent.command_id);
	decoded_intent.kind == intent.kind or TESTFAIL;
	decoded_intent.entity_ids == intent.entity_ids or TESTFAIL;
	TESTEQUALS(decoded_intent.target, intent.target);

	auto invalid_kind = encoded_intent;
	invalid_kind[8] = 0;
	invalid_kind[9] = 4;
	TESTTHROWS(decode_command_intent(invalid_kind));

	auto invalid_flags = encoded_intent;
	invalid_flags[11] = 1;
	TESTTHROWS(decode_command_intent(invalid_flags));

	auto excessive_entities = encoded_intent;
	excessive_entities[12] = 0;
	excessive_entities[13] = 0;
	excessive_entities[14] = 4;
	excessive_entities[15] = 1;
	TESTTHROWS(decode_command_intent(excessive_entities));

	auto truncated_intent = encoded_intent;
	truncated_intent.pop_back();
	TESTTHROWS(decode_command_intent(truncated_intent));

	auto trailing_intent = encoded_intent;
	trailing_intent.push_back(0);
	TESTTHROWS(decode_command_intent(trailing_intent));

	CommandResult result{};
	result.command_id = intent.command_id;
	result.status = CommandStatus::REJECTED;
	result.world_revision = 7;
	result.simulation_time = time::time_t::from_double(12.5);
	result.reason = "entity is not owned by this session";
	auto decoded_result = decode_command_result(encode_command_result(result));
	TESTEQUALS(decoded_result.command_id, result.command_id);
	decoded_result.status == result.status or TESTFAIL;
	TESTEQUALS(decoded_result.world_revision, result.world_revision);
	TESTEQUALS(decoded_result.simulation_time, result.simulation_time);
	TESTEQUALS(decoded_result.reason, result.reason);

	auto encoded_result = encode_command_result(result);
	auto invalid_status = encoded_result;
	invalid_status[9] = 3;
	TESTTHROWS(decode_command_result(invalid_status));

	auto invalid_result_flags = encoded_result;
	invalid_result_flags[11] = 1;
	TESTTHROWS(decode_command_result(invalid_result_flags));

	auto truncated_result = encoded_result;
	truncated_result.pop_back();
	TESTTHROWS(decode_command_result(truncated_result));

	auto trailing_result = encoded_result;
	trailing_result.push_back(0);
	TESTTHROWS(decode_command_result(trailing_result));

	CommandResult oversized_reason{};
	oversized_reason.reason = std::string(MAX_REJECTION_REASON_SIZE + 1, 'x');
	TESTTHROWS(encode_command_result(oversized_reason));

	WorldState state{};
	state.base_revision = 4;
	state.world_revision = 5;
	state.simulation_time = time::time_t::from_double(3.25);
	state.map_size = 128;
	state.local_player_id = 2;
	state.local_kill_count = 3;
	state.players = {
		{1, PlayerRole::HUNTER},
		{2, PlayerRole::BUILDER, false},
	};
	state.entities = {
		{100, 1, "fvd.data.entities.Horse", {10.5, 20.25, 0}, 17, 25},
		{200, 2, "fvd.data.entities.Foundation", {-1.0, 2.0, 3.0}},
		{201, 2, "fvd.data.entities.Tower", {12.0, 20.0, 0}, 100, 100, 2},
		{202, 1, "fvd.data.entities.Tower", {13.0, 20.0, 0}, 100, 100},
	};
	state.reservations = {{{10, 20}, 300}};
	state.constructions = {{
		7, 2, 300, {10, 20}, ConstructionPhase::PLANNED,
		time::time_t::from_double(4.5), 200}};
	state.attack_orders = {{
		100, {12.0, 21.0, 0}, 300, false, true}};
	state.arrows = {{
		9, 2, 100, {10.0, 20.0, 1.3}, {10.5, 20.25, 0.45},
		time::time_t::from_double(3.25), time::time_t::from_double(3.45)}};
	auto decoded_state = decode_world_state(encode_world_state(state));
	TESTEQUALS(decoded_state.base_revision, state.base_revision);
	TESTEQUALS(decoded_state.world_revision, state.world_revision);
	TESTEQUALS(decoded_state.simulation_time, state.simulation_time);
	TESTEQUALS(decoded_state.map_size, state.map_size);
	TESTEQUALS(decoded_state.local_player_id, state.local_player_id);
	TESTEQUALS(decoded_state.local_kill_count, state.local_kill_count);
	TESTEQUALS(decoded_state.players.size(), state.players.size());
	for (size_t i = 0; i < state.players.size(); ++i) {
		TESTEQUALS(decoded_state.players[i].player_id, state.players[i].player_id);
		decoded_state.players[i].role == state.players[i].role or TESTFAIL;
		TESTEQUALS(decoded_state.players[i].connected,
		           state.players[i].connected);
	}
	TESTEQUALS(decoded_state.entities.size(), state.entities.size());
	for (size_t i = 0; i < state.entities.size(); ++i) {
		TESTEQUALS(decoded_state.entities[i].entity_id, state.entities[i].entity_id);
		TESTEQUALS(decoded_state.entities[i].owner_id, state.entities[i].owner_id);
		TESTEQUALS(decoded_state.entities[i].definition, state.entities[i].definition);
		TESTEQUALS(decoded_state.entities[i].position, state.entities[i].position);
		TESTEQUALS(decoded_state.entities[i].health.has_value(),
		           state.entities[i].health.has_value());
		TESTEQUALS(decoded_state.entities[i].tower_kills.has_value(),
		           state.entities[i].tower_kills.has_value());
		if (state.entities[i].tower_kills) {
			TESTEQUALS(decoded_state.entities[i].tower_kills.value(),
			           state.entities[i].tower_kills.value());
		}
		if (state.entities[i].health) {
			TESTEQUALS(*decoded_state.entities[i].health, *state.entities[i].health);
			TESTEQUALS(*decoded_state.entities[i].maximum_health,
			           *state.entities[i].maximum_health);
		}
	}
	TESTEQUALS(decoded_state.reservations.size(), size_t{1});
	TESTEQUALS(decoded_state.reservations.front().tile, (coord::tile{10, 20}));
	TESTEQUALS(decoded_state.reservations.front().builder_id,
	           gamestate::entity_id_t{300});
	TESTEQUALS(decoded_state.constructions.size(), size_t{1});
	TESTEQUALS(decoded_state.constructions.front().order_id, uint64_t{7});
	decoded_state.constructions.front().phase == ConstructionPhase::PLANNED
		or TESTFAIL;
	TESTEQUALS(decoded_state.constructions.front().phase_end_time,
	           time::time_t::from_double(4.5));
	TESTEQUALS(decoded_state.constructions.front().site_entity_id,
	           gamestate::entity_id_t{200});
	TESTEQUALS(decoded_state.attack_orders.size(), size_t{1});
	TESTEQUALS(decoded_state.attack_orders.front().horse_id,
	           gamestate::entity_id_t{100});
	decoded_state.attack_orders.front().target_id.has_value() or TESTFAIL;
	TESTEQUALS(*decoded_state.attack_orders.front().target_id,
	           gamestate::entity_id_t{300});
	TESTEQUALS(decoded_state.attack_orders.front().focus_target, true);
	TESTEQUALS(decoded_state.arrows.size(), size_t{1});
	TESTEQUALS(decoded_state.arrows.front().arrow_id, uint64_t{9});
	TESTEQUALS(decoded_state.arrows.front().target_id,
	           gamestate::entity_id_t{100});
	TESTEQUALS(decoded_state.arrows.front().impact_time,
	           time::time_t::from_double(3.45));

	auto truncated_state = encode_world_state(state);
	truncated_state.resize(truncated_state.size() - 8);
	TESTTHROWS(decode_world_state(truncated_state));

	auto trailing_state = encode_world_state(state);
	trailing_state.push_back(0);
	TESTTHROWS(decode_world_state(trailing_state));
	auto unsupported_player_flags = encode_world_state(state);
	unsupported_player_flags[79] |= 2;
	TESTTHROWS(decode_world_state(unsupported_player_flags));
	auto invalid_health = state;
	invalid_health.entities.front().health = 26;
	TESTTHROWS(encode_world_state(invalid_health));
	auto foreign_local_player = state;
	foreign_local_player.local_player_id = 8;
	TESTTHROWS(encode_world_state(foreign_local_player));
	auto invalid_arrow = state;
	invalid_arrow.arrows.front().impact_time = invalid_arrow.arrows.front().launch_time;
	TESTTHROWS(encode_world_state(invalid_arrow));
	auto score_leak = state;
	score_leak.entities.back().tower_kills = 7;
	TESTTHROWS(encode_world_state(score_leak));
	auto nonscore_entity = state;
	nonscore_entity.entities.front().tower_kills = 1;
	TESTTHROWS(encode_world_state(nonscore_entity));

	WorldState maximum_state{};
	maximum_state.map_size = 1;
	maximum_state.local_player_id = 1;
	maximum_state.players = {{1, PlayerRole::HUNTER}};
	maximum_state.entities.resize(MAX_STATE_ENTITIES + 1);
	TESTTHROWS(encode_world_state(maximum_state));
	WorldState missing_map{};
	TESTTHROWS(encode_world_state(missing_map));

	SnapshotRequest snapshot_request{17};
	auto encoded_snapshot_request = encode_snapshot_request(snapshot_request);
	TESTEQUALS(encoded_snapshot_request.size(), size_t{8});
	TESTEQUALS(decode_snapshot_request(encoded_snapshot_request).last_applied_revision,
	           snapshot_request.last_applied_revision);
	encoded_snapshot_request.push_back(0);
	TESTTHROWS(decode_snapshot_request(encoded_snapshot_request));

	ParticipantDisconnected disconnected{3, DisconnectReason::CONNECTION_LOST};
	auto encoded_disconnected = encode_participant_disconnected(disconnected);
	TESTEQUALS(encoded_disconnected.size(), size_t{12});
	auto decoded_disconnected = decode_participant_disconnected(encoded_disconnected);
	TESTEQUALS(decoded_disconnected.player_id, disconnected.player_id);
	decoded_disconnected.reason == disconnected.reason or TESTFAIL;
	TESTTHROWS(encode_participant_disconnected(
		{0, DisconnectReason::CONNECTION_LOST}));
	TESTTHROWS(encode_participant_disconnected(
		{3, static_cast<DisconnectReason>(100)}));
	auto invalid_disconnect_flags = encoded_disconnected;
	invalid_disconnect_flags[11] = 1;
	TESTTHROWS(decode_participant_disconnected(invalid_disconnect_flags));
	auto trailing_disconnected = encoded_disconnected;
	trailing_disconnected.push_back(0);
	TESTTHROWS(decode_participant_disconnected(trailing_disconnected));
}

} // namespace openage::network::tests
