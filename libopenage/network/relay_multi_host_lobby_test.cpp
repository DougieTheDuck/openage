// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_multi_host_lobby.h"

#include <chrono>
#include <exception>
#include <string>
#include <thread>
#include <vector>

#include "network/relay_service.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

AssetDigest multi_lobby_digest() {
	AssetDigest digest{};
	for (size_t i = 0; i < digest.size(); ++i) {
		digest[i] = static_cast<uint8_t>(83 + i);
	}
	return digest;
}

void run_lobby_case(size_t people) {
	RelayRoomDirectory directory{1};
	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr relay_error{};
	std::jthread relay{[&]() {
		try {
			serve_relay_directory(listener, directory, 16);
		}
		catch (...) {
			relay_error = std::current_exception();
		}
	}};
	RelayMultiHostLobby host{
		"127.0.0.1", listener.get_port(), {},
		"Eight-Player FVD Room", "Host", people};
	TESTEQUALS(directory.room_count(), size_t{1});
	TESTEQUALS(host.participant_count(), size_t{1});
	TESTEQUALS(host.channels().size(), people - 1);

	std::vector<std::exception_ptr> errors(people - 1);
	std::vector<std::jthread> clients{};
	clients.reserve(people - 1);
	for (size_t index = 0; index < people - 1; ++index) {
		clients.emplace_back([&, index]() {
			try {
				auto socket = TcpConnection::connect(
					"127.0.0.1", listener.get_port());
				auto participant = join_relay_room_slot(
					socket, {}, host.room_id());
				TESTEQUALS(participant >= 2 and participant <= people, true);
				auto session = client_handshake(socket, {
					PROTOCOL_MINIMUM_COMPATIBLE_MINOR,
					"Builder " + std::to_string(index),
					"fvd-multi-room-test",
					multi_lobby_digest()});
				ParticipantSnapshot last{};
				size_t snapshots{0};
				while (true) {
					auto frame = socket.receive_frame();
					frame.has_value() or TESTFAIL;
					session.validate_incoming(*frame);
					if (frame->message_type == MessageType::PARTICIPANT_SNAPSHOT) {
						auto roster = decode_participant_snapshot(frame->payload);
						TESTEQUALS(roster.revision > last.revision, true);
						TESTEQUALS(roster.participants.size() >= 2, true);
						last = std::move(roster);
						++snapshots;
						continue;
					}
					frame->message_type == MessageType::START_MATCH or TESTFAIL;
					auto start = decode_start_match(frame->payload);
					validate_start_match_against_roster(last, start);
					TESTEQUALS(start.assignments.size(), people);
					TESTEQUALS(last.participants.size(), people);
					TESTEQUALS(snapshots > 0, true);
					session.bind_match_player(start, participant);
					TESTEQUALS(session.get_identity().player_id >= 1, true);
					TESTEQUALS(session.get_identity().player_id <= people, true);
					break;
				}
			}
			catch (...) {
				errors[index] = std::current_exception();
			}
		});
	}

	bool admitted = false;
	for (size_t attempt = 0; attempt < 1200; ++attempt) {
		host.poll_admissions(multi_lobby_digest());
		if (host.participant_count() == people) {
			admitted = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{5});
	}
	TESTEQUALS(admitted, true);
	TESTEQUALS(directory.room_count(), size_t{0});
	auto assignment = host.start();
	TESTEQUALS(assignment.assignments.size(), people);
	size_t hunters = 0;
	for (const auto &entry : assignment.assignments) {
		if (entry.role == PlayerRole::HUNTER) ++hunters;
	}
	TESTEQUALS(hunters, size_t{1});
	for (auto &client : clients) client.join();
	for (const auto &err : errors) if (err) std::rethrow_exception(err);
	host.close();
	listener.close();
	relay.join();
	if (relay_error) std::rethrow_exception(relay_error);
}

} // namespace

void relay_multi_host_lobby() {
	run_lobby_case(3);
	run_lobby_case(8);
}

} // namespace openage::network::tests
