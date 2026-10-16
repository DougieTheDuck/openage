// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <optional>

#include "gamestate/types.h"
#include "network/messages.h"
#include "network/session.h"

namespace openage::network {

class TcpConnection;

/**
 * Create a handshake frame with the required zero session and sequence.
 */
Frame make_client_hello_frame(const ClientHello &hello);

/**
 * Generate a process-unique, nonzero random session ID.
 *
 * Session IDs detect stale messages and routing mistakes; they are not
 * authentication credentials.
 */
uint64_t generate_session_id();

/**
 * Receive and evaluate one client handshake on the host.
 *
 * @param connection Newly accepted TCP connection.
 * @param assigned_player Lobby-assigned player for this participant.
 * @param required_asset_digest Digest required by the authoritative host.
 * @return Accepted connection session, or nullopt after sending ServerReject.
 * @throws Error for malformed or missing handshake frames.
 */
std::optional<ConnectionSession> host_handshake(
	TcpConnection &connection,
	gamestate::player_id_t assigned_player,
	const AssetDigest &required_asset_digest);

/**
 * Send a client handshake and receive the host-assigned identity.
 *
 * @throws Error for rejection, malformed replies or a closed connection.
 */
ConnectionSession client_handshake(TcpConnection &connection,
                                   const ClientHello &hello);

} // namespace openage::network
