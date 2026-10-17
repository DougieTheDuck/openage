// Copyright 2026 the openage authors. See copying.md for legal info.

#include "connection_mailbox.h"

#include <type_traits>
#include <utility>

#include "error/error.h"

namespace openage::network {

OutboundMailbox::OutboundMailbox(size_t max_pending) :
	messages{max_pending} {}

bool OutboundMailbox::try_push(OutboundMessage message) {
	return this->messages.try_push(std::move(message));
}

std::vector<OutboundMessage> OutboundMailbox::drain(size_t maximum_count) {
	return this->messages.drain(maximum_count);
}

size_t OutboundMailbox::size() const {
	return this->messages.size();
}

size_t OutboundMailbox::capacity() const {
	return this->messages.capacity();
}

ConnectionEventMailbox::ConnectionEventMailbox(size_t max_pending) :
	events{max_pending} {}

bool ConnectionEventMailbox::try_push(ConnectionClosed closed) {
	if (closed.identity.session_id == 0 or closed.identity.player_id == 0) {
		throw Error{MSG(err) << "FVD connection event requires an accepted session identity"};
	}
	if (not is_known_disconnect_reason(closed.reason)) {
		throw Error{MSG(err) << "Unknown FVD disconnect reason "
		                     << static_cast<uint16_t>(closed.reason)};
	}
	return this->events.try_push(std::move(closed));
}

std::vector<ConnectionClosed> ConnectionEventMailbox::drain(
	size_t maximum_count) {
	return this->events.drain(maximum_count);
}

size_t ConnectionEventMailbox::size() const {
	return this->events.size();
}

Frame make_outbound_frame(ConnectionSession &session,
                          const OutboundMessage &message) {
	return std::visit(
		[&session](const auto &typed_message) -> Frame {
			using message_t = std::decay_t<decltype(typed_message)>;
			if constexpr (std::is_same_v<message_t, CommandResult>) {
				return session.make_frame(
					MessageType::COMMAND_RESULT,
					encode_command_result(typed_message));
			}
			else if constexpr (std::is_same_v<message_t, WorldSnapshotMessage>) {
				return session.make_frame(
					MessageType::WORLD_SNAPSHOT,
					encode_world_state(typed_message.state));
			}
			else if constexpr (std::is_same_v<message_t, WorldDeltaMessage>) {
				return session.make_frame(
					MessageType::WORLD_DELTA,
					encode_world_state(typed_message.state));
			}
			else {
				return session.make_frame(
					MessageType::PARTICIPANT_DISCONNECTED,
					encode_participant_disconnected(typed_message));
			}
		},
		message);
}

} // namespace openage::network
