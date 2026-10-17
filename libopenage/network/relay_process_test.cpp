// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>

#include "error/error.h"
#include "network/handshake.h"
#include "network/messages.h"
#include "network/session.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

RelayToken process_token(uint8_t seed) {
	RelayToken token{};
	for (size_t i = 0; i < token.size(); ++i) {
		token[i] = static_cast<uint8_t>(seed + i);
	}
	return token;
}

RelayPairCredentials process_credentials() {
	return {
		23,
		2,
		process_token(11),
		process_token(111),
	};
}

RelayConnect process_connect(RelayRole role) {
	auto credentials = process_credentials();
	return {
		credentials.match_id,
		credentials.player_id,
		role,
		role == RelayRole::HOST ? credentials.host_token : credentials.guest_token,
	};
}

AssetDigest process_asset_digest() {
	AssetDigest digest{};
	for (size_t i = 0; i < digest.size(); ++i) {
		digest[i] = static_cast<uint8_t>(i + 31);
	}
	return digest;
}

std::string required_process_environment(const char *name) {
	auto value = std::getenv(name);
	if (value == nullptr or value[0] == '\0') {
		throw Error{MSG(err) << "Relay process test requires " << name};
	}
	return value;
}

uint16_t relay_process_port() {
	auto value = required_process_environment("OPENAGE_FVD_RELAY_TEST_PORT");
	unsigned int port{};
	auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
	if (parsed.ec != std::errc{}
	    or parsed.ptr != value.data() + value.size()
	    or port == 0
	    or port > (std::numeric_limits<uint16_t>::max)()) {
		throw Error{MSG(err) << "Invalid relay process test port " << value};
	}
	return static_cast<uint16_t>(port);
}

void announce_relay_process_port(uint16_t port) {
	auto path = required_process_environment("OPENAGE_FVD_RELAY_TEST_READY_FILE");
	std::ofstream ready{path, std::ios::trunc};
	if (not ready) {
		throw Error{MSG(err) << "Could not open relay process test ready file " << path};
	}
	ready << port << '\n';
	ready.close();
	if (not ready) {
		throw Error{MSG(err) << "Could not write relay process test ready file " << path};
	}
}

void run_relay_process() {
	auto listener = TcpListener::listen("127.0.0.1", 0);
	announce_relay_process_port(listener.get_port());
	forward_relay_pair(accept_relay_pair(listener, process_credentials()));
}

void run_relay_host_process() {
	auto connection = connect_relay(
		"127.0.0.1", relay_process_port(), process_connect(RelayRole::HOST));
	auto accepted = host_handshake(connection, 2, process_asset_digest());
	accepted.has_value() or TESTFAIL;
	auto &session = *accepted;

	auto command_frame = connection.receive_frame();
	command_frame.has_value() or TESTFAIL;
	session.validate_incoming(*command_frame);
	command_frame->message_type == MessageType::COMMAND_INTENT or TESTFAIL;
	auto command = decode_command_intent(command_frame->payload);
	TESTEQUALS(command.command_id, uint64_t{71});
	command.kind == CommandKind::MOVE or TESTFAIL;
	command.entity_ids == std::vector<gamestate::entity_id_t>({901, 902}) or TESTFAIL;

	CommandResult result{};
	result.command_id = command.command_id;
	result.status = CommandStatus::ACCEPTED;
	result.world_revision = 9;
	result.simulation_time = time::time_t::from_int(4);
	connection.send_frame(session.make_frame(
		MessageType::COMMAND_RESULT,
		encode_command_result(result)));
	TESTEQUALS(connection.receive_frame().has_value(), false);
}

void run_relay_guest_process() {
	auto connection = connect_relay(
		"127.0.0.1", relay_process_port(), process_connect(RelayRole::GUEST));
	ClientHello hello{
		PROTOCOL_MINIMUM_COMPATIBLE_MINOR,
		"RelayGuest",
		"fvd-relay-process-test",
		process_asset_digest(),
	};
	auto session = client_handshake(connection, hello);
	TESTEQUALS(session.get_identity().player_id, gamestate::player_id_t{2});

	CommandIntent command{};
	command.command_id = 71;
	command.kind = CommandKind::MOVE;
	command.entity_ids = {901, 902};
	command.target = {
		coord::phys_t::from_int(12),
		coord::phys_t::from_int(8),
		coord::phys_t::from_int(0),
	};
	connection.send_frame(session.make_frame(
		MessageType::COMMAND_INTENT,
		encode_command_intent(command)));

	auto result_frame = connection.receive_frame();
	result_frame.has_value() or TESTFAIL;
	session.validate_incoming(*result_frame);
	result_frame->message_type == MessageType::COMMAND_RESULT or TESTFAIL;
	auto result = decode_command_result(result_frame->payload);
	TESTEQUALS(result.command_id, command.command_id);
	result.status == CommandStatus::ACCEPTED or TESTFAIL;
	TESTEQUALS(result.world_revision, uint64_t{9});
	TESTEQUALS(result.simulation_time, time::time_t::from_int(4));
	connection.close();
}

} // namespace

void relay_process_worker() {
	auto role = required_process_environment("OPENAGE_FVD_RELAY_TEST_ROLE");
	if (role == "relay") {
		run_relay_process();
		return;
	}
	if (role == "host") {
		run_relay_host_process();
		return;
	}
	if (role == "guest") {
		run_relay_guest_process();
		return;
	}
	throw Error{MSG(err) << "Unknown relay process test role " << role};
}

} // namespace openage::network::tests
