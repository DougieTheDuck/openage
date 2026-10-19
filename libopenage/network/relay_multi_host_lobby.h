// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "network/handshake.h"
#include "network/lobby.h"
#include "network/relay.h"
#include "network/session.h"
#include "network/transport.h"

namespace openage::network {

/**
 * A host-side multi-guest relay admission manager.
 *
 * Creates a single discoverable room backed by up to seven independent
 * outbound TCP channels. Poll admissions on the lobby owner thread; no
 * simulation or renderer calls occur here. Existing two-peer rooms continue
 * to use RelayLobbyClient until the multi-guest gameplay handoff is wired.
 */
class RelayMultiHostLobby {
public:
	struct Peer {
		uint64_t participant_id{0};
		std::shared_ptr<TcpConnection> socket{};
		std::unique_ptr<ConnectionSession> session{};
		bool admitted{false};
	};

	RelayMultiHostLobby(const std::string &address,
	                    uint16_t port,
	                    const RelayToken &host_token,
	                    const std::string &room_name,
	                    const std::string &host_name,
	                    size_t player_capacity = 8);
	~RelayMultiHostLobby();
	RelayMultiHostLobby(const RelayMultiHostLobby &) = delete;
	RelayMultiHostLobby &operator=(const RelayMultiHostLobby &) = delete;

	uint64_t room_id() const;
	size_t poll_admissions(const AssetDigest &digest);
	size_t participant_count() const;
	ParticipantSnapshot roster() const;
	/** Send one immutable assignment on every admitted channel. */
	StartMatch start();

	/**
	 * Used after a pre-game Hunter authority transfer. The Hunter creates
	 * the new room, and callers map its newly authenticated participant IDs
	 * to the *existing* immutable match player slots. Never redraw roles.
	 * All new join credentials must be verified by the caller beforehand.
	 */
	StartMatch start_fixed(
		const std::map<uint64_t, gamestate::player_id_t> &player_slots);
	/** After start, each Peer::session is rebound to the assigned player. */
	const std::vector<Peer> &channels() const;
	void close();

private:
	void broadcast_roster();
	void broadcast_start(const StartMatch &assignment);
	uint64_t advertised_room_id{0};
	HostLobby lobby;
	std::vector<Peer> channels_by_slot{};
	bool started{false};
};

} // namespace openage::network
