// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <vector>

#include "gamestate/types.h"
#include "network/protocol.h"

namespace openage::network {

struct StartMatch;

/** Immutable identity assigned to one accepted match connection. */
struct SessionIdentity {
	uint64_t session_id{0};
	gamestate::player_id_t player_id{0};
};

/**
 * Protocol state for one accepted match connection.
 *
 * The connection thread owns this object.
 * It binds every decoded command to one player and enforces independent,
 * contiguous frame sequences in both directions.
 */
class ConnectionSession {
public:
	/** Create an accepted session with a nonzero session and player ID. */
	explicit ConnectionSession(
		SessionIdentity identity,
		ProtocolVersion protocol_version = {PROTOCOL_MAJOR, PROTOCOL_MINOR});

	/** Get the host-assigned connection identity. */
	const SessionIdentity &get_identity() const;

	/**
	 * Commit this accepted connection's final player slot after StartMatch.
	 * Keeps the session ID and both frame sequence counters unchanged.
	 * This is permitted exactly once, and only for an assigned participant.
	 */
	void bind_match_player(const StartMatch &start, uint64_t participant_id);

	/** Get the protocol version negotiated during the handshake. */
	const ProtocolVersion &get_protocol_version() const;

	/**
	 * Create the next outbound frame for this session.
	 *
	 * @throws Error if the message is handshake-only, the payload is too large
	 * or the sequence space is exhausted.
	 */
	Frame make_frame(MessageType message_type,
	                 std::vector<uint8_t> payload = {});

	/**
	 * Validate and consume the next inbound frame.
	 *
	 * @throws Error if protocol version, session ID, message phase or sequence
	 * does not match this connection.
	 */
	void validate_incoming(const Frame &frame);

private:
	SessionIdentity identity;
	bool match_player_bound{false};
	const ProtocolVersion protocol_version;
	uint64_t next_incoming_sequence;
	uint64_t next_outgoing_sequence;
};

} // namespace openage::network
