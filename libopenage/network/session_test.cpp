// Copyright 2026 the openage authors. See copying.md for legal info.

#include "session.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "testing/testing.h"

namespace openage::network::tests {

void session() {
	TESTTHROWS(ConnectionSession({0, 2}));
	TESTTHROWS(ConnectionSession({42, 0}));

	ConnectionSession connection{{42, 3}};
	TESTEQUALS(connection.get_identity().session_id, uint64_t{42});
	TESTEQUALS(connection.get_identity().player_id, gamestate::player_id_t{3});

	auto first = connection.make_frame(MessageType::COMMAND_RESULT, {1, 2});
	TESTEQUALS(first.session_id, uint64_t{42});
	TESTEQUALS(first.sequence, uint64_t{1});
	first.payload == std::vector<uint8_t>({1, 2}) or TESTFAIL;
	auto second = connection.make_frame(MessageType::WORLD_DELTA);
	TESTEQUALS(second.sequence, uint64_t{2});
	TESTTHROWS(connection.make_frame(MessageType::SERVER_ACCEPT));
	TESTTHROWS(connection.make_frame(static_cast<MessageType>(100)));

	Frame incoming{};
	incoming.message_type = MessageType::COMMAND_INTENT;
	incoming.session_id = 42;
	incoming.sequence = 1;
	connection.validate_incoming(incoming);

	auto duplicate = incoming;
	TESTTHROWS(connection.validate_incoming(duplicate));

	auto next = incoming;
	next.sequence = 2;
	connection.validate_incoming(next);

	auto skipped = incoming;
	skipped.sequence = 4;
	TESTTHROWS(connection.validate_incoming(skipped));

	auto wrong_session = incoming;
	wrong_session.sequence = 3;
	wrong_session.session_id = 43;
	TESTTHROWS(connection.validate_incoming(wrong_session));

	auto wrong_major = incoming;
	wrong_major.sequence = 3;
	wrong_major.protocol_major += 1;
	TESTTHROWS(connection.validate_incoming(wrong_major));

	auto wrong_minor = incoming;
	wrong_minor.sequence = 3;
	wrong_minor.protocol_minor += 1;
	TESTTHROWS(connection.validate_incoming(wrong_minor));

	auto handshake = incoming;
	handshake.sequence = 3;
	handshake.message_type = MessageType::CLIENT_HELLO;
	TESTTHROWS(connection.validate_incoming(handshake));

	auto unknown = incoming;
	unknown.sequence = 3;
	unknown.message_type = static_cast<MessageType>(100);
	TESTTHROWS(connection.validate_incoming(unknown));

	std::vector<uint8_t> oversized(MAX_PAYLOAD_SIZE + 1);
	TESTTHROWS(connection.make_frame(MessageType::WORLD_SNAPSHOT, std::move(oversized)));
	auto third_outbound = connection.make_frame(MessageType::PING);
	TESTEQUALS(third_outbound.sequence, uint64_t{3});

	auto third = incoming;
	third.sequence = 3;
	connection.validate_incoming(third);
}

} // namespace openage::network::tests
