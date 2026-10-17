// Copyright 2026 the openage authors. See copying.md for legal info.

#include "session.h"

#include <limits>
#include <utility>

#include "error/error.h"

namespace openage::network {

namespace {

bool is_pre_session_message(MessageType message_type) {
	return message_type == MessageType::CLIENT_HELLO
	    or message_type == MessageType::SERVER_ACCEPT
	    or message_type == MessageType::SERVER_REJECT
	    or message_type == MessageType::RELAY_CONNECT
	    or message_type == MessageType::RELAY_READY
	    or message_type == MessageType::RELAY_REJECT;
}

void require_session_message(MessageType message_type) {
	if (not is_known_message_type(message_type)) {
		throw Error{MSG(err) << "Unknown FVD network message type "
		                     << static_cast<uint16_t>(message_type)};
	}
	if (is_pre_session_message(message_type)) {
		throw Error{MSG(err) << "Pre-session message " << static_cast<uint16_t>(message_type)
		                     << " is invalid after FVD session acceptance"};
	}
}

} // namespace

ConnectionSession::ConnectionSession(SessionIdentity identity,
                                     ProtocolVersion protocol_version) :
	identity{identity},
	protocol_version{protocol_version},
	next_incoming_sequence{1},
	next_outgoing_sequence{1} {
	if (identity.session_id == 0) {
		throw Error{MSG(err) << "Accepted FVD session ID must be nonzero"};
	}
	if (identity.player_id == 0) {
		throw Error{MSG(err) << "Accepted FVD session player ID must be nonzero"};
	}
	if (protocol_version.major != PROTOCOL_MAJOR
	    or protocol_version.minor < PROTOCOL_MINIMUM_COMPATIBLE_MINOR
	    or protocol_version.minor > PROTOCOL_MINOR) {
		throw Error{MSG(err) << "Incompatible negotiated FVD protocol version "
		                     << protocol_version.major << "." << protocol_version.minor};
	}
}

const SessionIdentity &ConnectionSession::get_identity() const {
	return this->identity;
}

const ProtocolVersion &ConnectionSession::get_protocol_version() const {
	return this->protocol_version;
}

Frame ConnectionSession::make_frame(MessageType message_type,
                                    std::vector<uint8_t> payload) {
	require_session_message(message_type);
	if (payload.size() > MAX_PAYLOAD_SIZE) {
		throw Error{MSG(err) << "FVD session payload exceeds "
		                     << MAX_PAYLOAD_SIZE << " bytes"};
	}
	if (this->next_outgoing_sequence == (std::numeric_limits<uint64_t>::max)()) {
		throw Error{MSG(err) << "FVD outbound session sequence is exhausted"};
	}

	Frame frame{};
	frame.protocol_major = this->protocol_version.major;
	frame.protocol_minor = this->protocol_version.minor;
	frame.message_type = message_type;
	frame.session_id = this->identity.session_id;
	frame.sequence = this->next_outgoing_sequence;
	frame.payload = std::move(payload);
	this->next_outgoing_sequence += 1;
	return frame;
}

void ConnectionSession::validate_incoming(const Frame &frame) {
	if (frame.protocol_major != this->protocol_version.major
	    or frame.protocol_minor != this->protocol_version.minor) {
		throw Error{MSG(err) << "FVD session protocol version mismatch: received "
		                     << frame.protocol_major << "." << frame.protocol_minor
		                     << ", expected " << this->protocol_version.major << "."
		                     << this->protocol_version.minor};
	}
	require_session_message(frame.message_type);
	if (frame.session_id != this->identity.session_id) {
		throw Error{MSG(err) << "FVD frame session ID " << frame.session_id
		                     << " does not match connection session "
		                     << this->identity.session_id};
	}
	if (frame.sequence != this->next_incoming_sequence) {
		throw Error{MSG(err) << "FVD frame sequence " << frame.sequence
		                     << " does not match expected sequence "
		                     << this->next_incoming_sequence};
	}
	if (this->next_incoming_sequence == (std::numeric_limits<uint64_t>::max)()) {
		throw Error{MSG(err) << "FVD inbound session sequence is exhausted"};
	}
	this->next_incoming_sequence += 1;
}

} // namespace openage::network
