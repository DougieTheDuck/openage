// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gamestate/types.h"
#include "network/transport.h"

namespace openage::network {

/** Protocol minor version that introduced relay admission messages. */
constexpr uint16_t RELAY_PROTOCOL_MINOR = 1;

/** Size of a lobby-issued relay admission token. */
constexpr size_t RELAY_TOKEN_SIZE = 32;

/** A single-use 256-bit bearer credential issued by the lobby. */
using RelayToken = std::array<uint8_t, RELAY_TOKEN_SIZE>;

/** Side of one relayed host-to-guest connection. */
enum class RelayRole : uint16_t {
	HOST = 1,
	GUEST = 2,
};

/** Admission request sent before the ordinary match handshake. */
struct RelayConnect {
	uint64_t match_id{};
	gamestate::player_id_t player_id{};
	RelayRole role{RelayRole::GUEST};
	RelayToken token{};
};

/** Credentials provisioned by the lobby for one host-to-guest pair. */
struct RelayPairCredentials {
	uint64_t match_id{};
	gamestate::player_id_t player_id{};
	RelayToken host_token{};
	RelayToken guest_token{};
};

/** The two authenticated connections for one relay pair. */
struct RelayPair {
	TcpConnection host{};
	TcpConnection guest{};
};

/** Encode and validate a relay admission payload. */
std::vector<uint8_t> encode_relay_connect(const RelayConnect &connect);

/** Decode and validate a relay admission payload. */
RelayConnect decode_relay_connect(const std::vector<uint8_t> &payload);

/**
 * Open an outbound relay connection and authenticate one side of a pair.
 *
 * The returned connection is ready for the ordinary FVD host/client
 * handshake. Relay admission frames are consumed by this function.
 */
TcpConnection connect_relay(const std::string &address,
                            uint16_t port,
                            const RelayConnect &connect);

/**
 * Accept both sides of one provisioned pair in either arrival order.
 *
 * Malformed and unauthorized connections are rejected without consuming a
 * role. Credentials are consumed by returning the pair and must not be reused.
 */
RelayPair accept_relay_pair(TcpListener &listener,
                            const RelayPairCredentials &credentials);

/**
 * Forward complete protocol frames in both directions until either peer
 * disconnects.
 */
void forward_relay_pair(RelayPair pair);

} // namespace openage::network
