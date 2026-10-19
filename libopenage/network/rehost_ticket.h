// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "network/lobby.h"
#include "network/session.h"

namespace openage::network {

/**
 * A one-time rejoin credential delivered privately over the original,
 * authenticated lobby session. It is never a public relay-room password.
 */
using RehostTicket = std::array<uint8_t, 32>;

struct RehostOffer {
	uint64_t original_lobby_revision{0};
	uint64_t room_id{0};
	uint64_t authority_epoch{0};
	gamestate::player_id_t original_player_id{0};
	RehostTicket ticket{};
};

struct RehostProof {
	uint64_t original_lobby_revision{0};
	uint64_t room_id{0};
	uint64_t authority_epoch{0};
	gamestate::player_id_t original_player_id{0};
	RehostTicket ticket{};
};

/** Fixed-width (64 bytes) network-order handoff payloads. */
std::vector<uint8_t> encode_rehost_offer(const RehostOffer &offer);
RehostOffer decode_rehost_offer(const std::vector<uint8_t> &bytes);
std::vector<uint8_t> encode_rehost_proof(const RehostProof &proof);
RehostProof decode_rehost_proof(const std::vector<uint8_t> &bytes);
RehostProof make_rehost_proof(const RehostOffer &offer);

/**
 * The Hunter owns this book for the duration of the pre-game rehost.
 * Every admitted Builder has one random, single-use ticket tied to its
 * immutable player ID, old lobby revision, destination room and new epoch.
 *
 * Redeeming a ticket MUST occur after an accepted *new* TCP handshake,
 * but before the participant is added to the new lobby.
 * The original Builder-host may join as a guest, but cannot take over the
 * Hunter's player slot.
 */
class RehostTicketBook {
public:
	explicit RehostTicketBook(const StartMatch &original, uint64_t room_id,
	                          uint64_t successor_epoch = 2);

	RehostOffer offer_for(gamestate::player_id_t old_player_id) const;
	/** True once and only once per player, bound to the fresh session ID. */
	bool redeem(const RehostProof &proof, SessionIdentity accepted_session);
	bool has_redeemed(gamestate::player_id_t old_player_id) const;
	bool complete() const;
	size_t expected_guests() const;
	/** Map new relay participant IDs to their OLD player IDs. */
	const std::map<uint64_t, gamestate::player_id_t> &new_participant_slots() const;

private:
	uint64_t original_revision{0};
	uint64_t target_room{0};
	uint64_t epoch{0};
	std::map<gamestate::player_id_t, RehostTicket> tickets{};
	std::map<gamestate::player_id_t, uint64_t> accepted_sessions{};
	std::map<uint64_t, gamestate::player_id_t> participant_slots{{1, 1}};
};

} // namespace openage::network
