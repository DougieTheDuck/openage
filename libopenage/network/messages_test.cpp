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
	state.positions = {
		{100, {10.5, 20.25, 0}},
		{200, {-1.0, 2.0, 3.0}},
	};
	auto decoded_state = decode_world_state(encode_world_state(state));
	TESTEQUALS(decoded_state.base_revision, state.base_revision);
	TESTEQUALS(decoded_state.world_revision, state.world_revision);
	TESTEQUALS(decoded_state.simulation_time, state.simulation_time);
	TESTEQUALS(decoded_state.positions.size(), state.positions.size());
	for (size_t i = 0; i < state.positions.size(); ++i) {
		TESTEQUALS(decoded_state.positions[i].entity_id, state.positions[i].entity_id);
		TESTEQUALS(decoded_state.positions[i].position, state.positions[i].position);
	}

	auto truncated_state = encode_world_state(state);
	truncated_state.resize(truncated_state.size() - 8);
	TESTTHROWS(decode_world_state(truncated_state));

	auto trailing_state = encode_world_state(state);
	trailing_state.push_back(0);
	TESTTHROWS(decode_world_state(trailing_state));

	WorldState maximum_state{};
	maximum_state.positions.resize(MAX_STATE_POSITIONS);
	auto maximum_payload = encode_world_state(maximum_state);
	maximum_payload.size() <= MAX_PAYLOAD_SIZE or TESTFAIL;
	maximum_state.positions.emplace_back();
	TESTTHROWS(encode_world_state(maximum_state));

	SnapshotRequest snapshot_request{17};
	auto encoded_snapshot_request = encode_snapshot_request(snapshot_request);
	TESTEQUALS(encoded_snapshot_request.size(), size_t{8});
	TESTEQUALS(decode_snapshot_request(encoded_snapshot_request).last_applied_revision,
	           snapshot_request.last_applied_revision);
	encoded_snapshot_request.push_back(0);
	TESTTHROWS(decode_snapshot_request(encoded_snapshot_request));
}

} // namespace openage::network::tests
