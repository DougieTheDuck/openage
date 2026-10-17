// Copyright 2026 the openage authors. See copying.md for legal info.

#include "connection_mailbox.h"

#include <exception>
#include <thread>
#include <variant>

#include "network/handshake.h"
#include "network/transport.h"
#include "testing/testing.h"

namespace openage::network::tests {

namespace {

AssetDigest disconnect_test_digest() {
	AssetDigest digest{};
	for (size_t i = 0; i < digest.size(); ++i) {
		digest[i] = static_cast<uint8_t>(i + 17);
	}
	return digest;
}

ClientHello disconnect_test_hello(const char *name) {
	return {
		PROTOCOL_MINIMUM_COMPATIBLE_MINOR,
		name,
		"fvd-test",
		disconnect_test_digest(),
	};
}

} // namespace

void connection_mailbox() {
	TESTTHROWS(OutboundMailbox{0});
	OutboundMailbox outbound{2};
	TESTEQUALS(outbound.capacity(), size_t{2});
	CommandResult result{};
	result.command_id = 7;
	result.status = CommandStatus::ACCEPTED;
	result.world_revision = 1;
	outbound.try_push(result) or TESTFAIL;
	WorldDeltaMessage delta{};
	delta.state.base_revision = 0;
	delta.state.world_revision = 1;
	outbound.try_push(delta) or TESTFAIL;
	outbound.try_push(ParticipantDisconnected{
		3, DisconnectReason::CONNECTION_LOST})
		and TESTFAIL;

	auto messages = outbound.drain(2);
	TESTEQUALS(messages.size(), size_t{2});
	std::holds_alternative<CommandResult>(messages[0]) or TESTFAIL;
	std::holds_alternative<WorldDeltaMessage>(messages[1]) or TESTFAIL;
	ConnectionSession frame_session{{42, 2}};
	auto result_frame = make_outbound_frame(frame_session, messages[0]);
	result_frame.message_type == MessageType::COMMAND_RESULT or TESTFAIL;
	TESTEQUALS(result_frame.sequence, uint64_t{1});
	TESTEQUALS(decode_command_result(result_frame.payload).command_id, uint64_t{7});
	auto delta_frame = make_outbound_frame(frame_session, messages[1]);
	delta_frame.message_type == MessageType::WORLD_DELTA or TESTFAIL;
	TESTEQUALS(delta_frame.sequence, uint64_t{2});
	TESTEQUALS(decode_world_state(delta_frame.payload).world_revision, uint64_t{1});
	WorldSnapshotMessage snapshot{};
	snapshot.state.world_revision = 1;
	outbound.try_push(snapshot) or TESTFAIL;
	outbound.try_push(ParticipantDisconnected{
		3, DisconnectReason::LEFT_MATCH})
		or TESTFAIL;
	messages = outbound.drain(2);
	auto snapshot_frame = make_outbound_frame(frame_session, messages[0]);
	snapshot_frame.message_type == MessageType::WORLD_SNAPSHOT or TESTFAIL;
	auto disconnected_frame = make_outbound_frame(frame_session, messages[1]);
	disconnected_frame.message_type == MessageType::PARTICIPANT_DISCONNECTED
		or TESTFAIL;
	TESTEQUALS(
		decode_participant_disconnected(disconnected_frame.payload).player_id,
		gamestate::player_id_t{3});

	TESTTHROWS(ConnectionEventMailbox{0});
	ConnectionEventMailbox events{2};
	TESTTHROWS(events.try_push({{}, DisconnectReason::CONNECTION_LOST}));
	TESTTHROWS(events.try_push({{42, 2}, static_cast<DisconnectReason>(100)}));
	events.try_push({{42, 2}, DisconnectReason::LEFT_MATCH}) or TESTFAIL;
	auto closed = events.drain(1);
	TESTEQUALS(closed.size(), size_t{1});
	TESTEQUALS(closed.front().identity.player_id, gamestate::player_id_t{2});
	closed.front().reason == DisconnectReason::LEFT_MATCH or TESTFAIL;

	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr server_error{};
	std::jthread server{[&]() {
		try {
			auto leaving_connection = listener.accept();
			auto leaving_session = host_handshake(
				leaving_connection, 2, disconnect_test_digest());
			leaving_session.has_value() or TESTFAIL;

			auto observer_connection = listener.accept();
			auto observer_session = host_handshake(
				observer_connection, 3, disconnect_test_digest());
			observer_session.has_value() or TESTFAIL;

			auto after_close = leaving_connection.receive_frame();
			not after_close.has_value() or TESTFAIL;
			ConnectionEventMailbox lifecycle{2};
			lifecycle.try_push({
				leaving_session->get_identity(),
				DisconnectReason::CONNECTION_LOST})
				or TESTFAIL;
			auto closures = lifecycle.drain(1);
			TESTEQUALS(closures.size(), size_t{1});

			OutboundMailbox observer_outbound{2};
			observer_outbound.try_push(ParticipantDisconnected{
				closures.front().identity.player_id,
				closures.front().reason})
				or TESTFAIL;
			auto notifications = observer_outbound.drain(1);
			TESTEQUALS(notifications.size(), size_t{1});
			observer_connection.send_frame(make_outbound_frame(
				*observer_session, notifications.front()));
		}
		catch (...) {
			server_error = std::current_exception();
		}
	}};

	auto leaving = TcpConnection::connect("127.0.0.1", listener.get_port());
	auto leaving_session = client_handshake(
		leaving, disconnect_test_hello("Leaving Builder"));
	TESTEQUALS(leaving_session.get_identity().player_id, gamestate::player_id_t{2});
	auto observer = TcpConnection::connect("127.0.0.1", listener.get_port());
	auto observer_session = client_handshake(
		observer, disconnect_test_hello("Observer Builder"));
	leaving.close();

	auto notification_frame = observer.receive_frame();
	notification_frame.has_value() or TESTFAIL;
	observer_session.validate_incoming(*notification_frame);
	notification_frame->message_type == MessageType::PARTICIPANT_DISCONNECTED
		or TESTFAIL;
	auto notification = decode_participant_disconnected(
		notification_frame->payload);
	TESTEQUALS(notification.player_id, gamestate::player_id_t{2});
	notification.reason == DisconnectReason::CONNECTION_LOST or TESTFAIL;

	server.join();
	if (server_error) {
		std::rethrow_exception(server_error);
	}
}

} // namespace openage::network::tests
