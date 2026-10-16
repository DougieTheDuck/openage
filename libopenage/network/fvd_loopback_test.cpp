// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_bridge.h"

#include <charconv>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "assets/mod_manager.h"
#include "config.h"
#include "error/error.h"
#include "event/event_loop.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/types.h"
#include "gamestate/entity_factory.h"
#include "gamestate/event/process_command.h"
#include "gamestate/event/send_command.h"
#include "gamestate/event/wait.h"
#include "gamestate/game.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/terrain_factory.h"
#include "network/messages.h"
#include "network/protocol.h"
#include "network/transport.h"
#include "testing/testing.h"
#include "util/fslike/directory.h"
#include "util/path.h"

namespace openage::network::tests {

namespace {

constexpr uint64_t TEST_SESSION_ID = 42;

void run_host(TcpListener &listener) {
	auto root = util::Path{
		std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<event::EventLoop>();
	loop->add_event_handler(std::make_shared<gamestate::event::SendCommandHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<gamestate::event::WaitHandler>());
	auto entities = std::make_shared<gamestate::EntityFactory>();
	auto terrain = std::make_shared<gamestate::TerrainFactory>();
	gamestate::Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto commander = std::make_shared<gamestate::event::Commander>(loop);

	std::optional<gamestate::entity_id_t> builder_id{};
	std::optional<gamestate::entity_id_t> other_builder_id{};
	for (const auto &[entity_id, entity] : state->get_game_entities()) {
		auto ownership = std::dynamic_pointer_cast<gamestate::component::Ownership>(
			entity->get_component(gamestate::component::component_t::OWNERSHIP));
		auto owner = ownership->get_owners().get(time::TIME_ZERO);
		if (owner == 2) {
			builder_id = entity_id;
		}
		else if (owner == 3) {
			other_builder_id = entity_id;
		}
	}
	builder_id.has_value() or TESTFAIL;
	other_builder_id.has_value() or TESTFAIL;

	auto connection = listener.accept();
	Frame snapshot_frame{};
	snapshot_frame.message_type = MessageType::WORLD_SNAPSHOT;
	snapshot_frame.session_id = TEST_SESSION_ID;
	snapshot_frame.sequence = 1;
	snapshot_frame.payload = encode_world_state(
		capture_positions(state, {*builder_id, *other_builder_id}, time::TIME_ZERO, 0, 0));
	connection.send_frame(snapshot_frame);

	auto command_frame = connection.receive_frame();
	command_frame.has_value() or TESTFAIL;
	command_frame->message_type == MessageType::COMMAND_INTENT or TESTFAIL;
	TESTEQUALS(command_frame->session_id, TEST_SESSION_ID);
	TESTEQUALS(command_frame->sequence, uint64_t{1});
	auto intent = decode_command_intent(command_frame->payload);

	std::optional<bool> accepted{};
	submit_command_intent(
		intent,
		2,
		loop,
		commander,
		state,
		time::TIME_ZERO,
		[&](bool result) { accepted = result; });
	loop->reach_time(time::TIME_ZERO, state);
	accepted.has_value() or TESTFAIL;

	CommandResult command_result{};
	command_result.command_id = intent.command_id;
	command_result.status = *accepted ? CommandStatus::ACCEPTED : CommandStatus::REJECTED;
	command_result.world_revision = *accepted ? 1 : 0;
	command_result.simulation_time = time::TIME_ZERO;
	command_result.reason = *accepted ? "" : "command rejected by authoritative simulation";
	Frame result_frame{};
	result_frame.message_type = MessageType::COMMAND_RESULT;
	result_frame.session_id = TEST_SESSION_ID;
	result_frame.sequence = 2;
	result_frame.payload = encode_command_result(command_result);
	connection.send_frame(result_frame);

	if (*accepted) {
		auto sample_time = time::time_t::from_int(30);
		loop->reach_time(sample_time, state);
		Frame delta_frame{};
		delta_frame.message_type = MessageType::WORLD_DELTA;
		delta_frame.session_id = TEST_SESSION_ID;
		delta_frame.sequence = 3;
		delta_frame.payload = encode_world_state(
			capture_positions(state, intent.entity_ids, sample_time, 0, 1));
		connection.send_frame(delta_frame);

		auto spoofed_frame = connection.receive_frame();
		spoofed_frame.has_value() or TESTFAIL;
		spoofed_frame->message_type == MessageType::COMMAND_INTENT or TESTFAIL;
		TESTEQUALS(spoofed_frame->session_id, TEST_SESSION_ID);
		TESTEQUALS(spoofed_frame->sequence, uint64_t{2});
		auto spoofed = decode_command_intent(spoofed_frame->payload);
		auto before = capture_positions(
			state, spoofed.entity_ids, sample_time, 0, 1);
		std::optional<bool> spoofed_accepted{};
		submit_command_intent(
			spoofed,
			2,
			loop,
			commander,
			state,
			sample_time,
			[&](bool result) { spoofed_accepted = result; });
		loop->reach_time(sample_time, state);
		spoofed_accepted.has_value() or TESTFAIL;
		TESTEQUALS(*spoofed_accepted, false);
		auto after = capture_positions(
			state, spoofed.entity_ids, sample_time, 0, 1);
		TESTEQUALS(after.positions.front().position, before.positions.front().position);

		CommandResult spoofed_result{};
		spoofed_result.command_id = spoofed.command_id;
		spoofed_result.status = CommandStatus::REJECTED;
		spoofed_result.world_revision = 1;
		spoofed_result.simulation_time = sample_time;
		spoofed_result.reason = "entity is not owned by this session";
		Frame spoofed_result_frame{};
		spoofed_result_frame.message_type = MessageType::COMMAND_RESULT;
		spoofed_result_frame.session_id = TEST_SESSION_ID;
		spoofed_result_frame.sequence = 4;
		spoofed_result_frame.payload = encode_command_result(spoofed_result);
		connection.send_frame(spoofed_result_frame);
	}
}

void run_guest(uint16_t port) {
	auto client = TcpConnection::connect("127.0.0.1", port);
	auto snapshot_frame = client.receive_frame();
	snapshot_frame.has_value() or TESTFAIL;
	snapshot_frame->message_type == MessageType::WORLD_SNAPSHOT or TESTFAIL;
	TESTEQUALS(snapshot_frame->session_id, TEST_SESSION_ID);
	TESTEQUALS(snapshot_frame->sequence, uint64_t{1});
	auto snapshot = decode_world_state(snapshot_frame->payload);
	TESTEQUALS(snapshot.positions.size(), 2);
	auto initial = snapshot.positions.front();
	auto other = snapshot.positions.back();
	auto target = coord::phys3{
		initial.position.ne,
		initial.position.se + coord::phys_t::from_int(3),
		initial.position.up};

	CommandIntent intent{};
	intent.command_id = 1;
	intent.kind = CommandKind::MOVE;
	intent.entity_ids = {initial.entity_id};
	intent.target = target;
	Frame command_frame{};
	command_frame.message_type = MessageType::COMMAND_INTENT;
	command_frame.session_id = TEST_SESSION_ID;
	command_frame.sequence = 1;
	command_frame.payload = encode_command_intent(intent);
	client.send_frame(command_frame);

	auto result_frame = client.receive_frame();
	result_frame.has_value() or TESTFAIL;
	result_frame->message_type == MessageType::COMMAND_RESULT or TESTFAIL;
	TESTEQUALS(result_frame->session_id, TEST_SESSION_ID);
	TESTEQUALS(result_frame->sequence, uint64_t{2});
	auto result = decode_command_result(result_frame->payload);
	TESTEQUALS(result.command_id, intent.command_id);
	result.status == CommandStatus::ACCEPTED or TESTFAIL;
	TESTEQUALS(result.world_revision, uint64_t{1});

	auto delta_frame = client.receive_frame();
	delta_frame.has_value() or TESTFAIL;
	delta_frame->message_type == MessageType::WORLD_DELTA or TESTFAIL;
	TESTEQUALS(delta_frame->session_id, TEST_SESSION_ID);
	TESTEQUALS(delta_frame->sequence, uint64_t{3});
	auto delta = decode_world_state(delta_frame->payload);
	TESTEQUALS(delta.base_revision, uint64_t{0});
	TESTEQUALS(delta.world_revision, uint64_t{1});
	TESTEQUALS(delta.positions.size(), 1);
	TESTEQUALS(delta.positions.front().entity_id, initial.entity_id);
	TESTEQUALS(delta.positions.front().position, target);

	CommandIntent spoofed{};
	spoofed.command_id = 2;
	spoofed.kind = CommandKind::MOVE;
	spoofed.entity_ids = {other.entity_id};
	spoofed.target = {
		other.position.ne,
		other.position.se + coord::phys_t::from_int(3),
		other.position.up};
	Frame spoofed_frame{};
	spoofed_frame.message_type = MessageType::COMMAND_INTENT;
	spoofed_frame.session_id = TEST_SESSION_ID;
	spoofed_frame.sequence = 2;
	spoofed_frame.payload = encode_command_intent(spoofed);
	client.send_frame(spoofed_frame);

	auto spoofed_result_frame = client.receive_frame();
	spoofed_result_frame.has_value() or TESTFAIL;
	spoofed_result_frame->message_type == MessageType::COMMAND_RESULT or TESTFAIL;
	TESTEQUALS(spoofed_result_frame->session_id, TEST_SESSION_ID);
	TESTEQUALS(spoofed_result_frame->sequence, uint64_t{4});
	auto spoofed_result = decode_command_result(spoofed_result_frame->payload);
	TESTEQUALS(spoofed_result.command_id, spoofed.command_id);
	spoofed_result.status == CommandStatus::REJECTED or TESTFAIL;
	TESTEQUALS(spoofed_result.world_revision, uint64_t{1});
	TESTEQUALS(spoofed_result.reason, "entity is not owned by this session");
}

std::string required_environment(const char *name) {
	auto value = std::getenv(name);
	if (value == nullptr or value[0] == '\0') {
		throw Error{MSG(err) << "Two-process network test requires " << name};
	}
	return value;
}

uint16_t process_test_port() {
	auto value = required_environment("OPENAGE_FVD_NETWORK_TEST_PORT");
	unsigned int port{};
	auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
	if (parsed.ec != std::errc{}
	    or parsed.ptr != value.data() + value.size()
	    or port == 0
	    or port > (std::numeric_limits<uint16_t>::max)()) {
		throw Error{MSG(err) << "Invalid two-process network test port " << value};
	}
	return static_cast<uint16_t>(port);
}

void announce_process_test_port(uint16_t port) {
	auto path = required_environment("OPENAGE_FVD_NETWORK_TEST_READY_FILE");
	std::ofstream ready{path, std::ios::trunc};
	if (not ready) {
		throw Error{MSG(err) << "Could not open two-process network test ready file " << path};
	}
	ready << port << '\n';
	ready.close();
	if (not ready) {
		throw Error{MSG(err) << "Could not write two-process network test ready file " << path};
	}
}

} // namespace

void fvd_loopback() {
	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr server_error{};
	std::jthread server{[&]() {
		try {
			run_host(listener);
		}
		catch (...) {
			server_error = std::current_exception();
			listener.close();
		}
	}};

	run_guest(listener.get_port());
	server.join();
	if (server_error) {
		std::rethrow_exception(server_error);
	}
}

void fvd_process_worker() {
	auto role = required_environment("OPENAGE_FVD_NETWORK_TEST_ROLE");
	if (role == "host") {
		auto listener = TcpListener::listen("127.0.0.1", 0);
		announce_process_test_port(listener.get_port());
		run_host(listener);
		return;
	}
	if (role == "guest") {
		run_guest(process_test_port());
		return;
	}
	throw Error{MSG(err) << "Unknown two-process network test role " << role};
}

} // namespace openage::network::tests
