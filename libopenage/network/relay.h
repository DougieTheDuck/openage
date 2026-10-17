// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
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
	uint64_t match_id{};
	gamestate::player_id_t player_id{};
	TcpConnection host{};
	TcpConnection guest{};
};

/**
 * Thread-safe registry of lobby-provisioned relay pairs awaiting admission.
 *
 * A pair leaves the registry permanently when both roles authenticate.
 * Cancelling a pending pair closes whichever side has already connected.
 */
class RelayPairRegistry {
public:
	/** Create a registry with a fixed upper bound on pending pairs. */
	explicit RelayPairRegistry(size_t max_pairs);

	/**
	 * Add credentials for one pair.
	 *
	 * Pair identities and tokens must be unique among pending pairs.
	 */
	void provision(const RelayPairCredentials &credentials);

	/**
	 * Authenticate one newly accepted connection.
	 *
	 * @return A completed pair when this connection supplies its second role,
	 * or nullopt after storing the first role or rejecting the connection.
	 */
	std::optional<RelayPair> admit(TcpConnection &connection);

	/** Admit a temporary connection. */
	std::optional<RelayPair> admit(TcpConnection &&connection);

	/** Close and remove a pair that has not completed admission. */
	bool cancel(uint64_t match_id, gamestate::player_id_t player_id);

	/** Close and remove every pair that has not completed admission. */
	size_t cancel_all();

	/** Number of provisioned pairs still awaiting one or both roles. */
	size_t pair_count() const;

	/** Number of authenticated connections held by pending pairs. */
	size_t connection_count() const;

private:
	struct PairKey {
		uint64_t match_id;
		gamestate::player_id_t player_id;

		bool operator<(const PairKey &other) const;
	};

	struct PendingPair {
		RelayPairCredentials credentials;
		std::optional<TcpConnection> host;
		std::optional<TcpConnection> guest;
	};

	const size_t max_pairs;
	mutable std::mutex mutex;
	std::map<PairKey, PendingPair> pairs;
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
