// Copyright 2026 the openage authors. See copying.md for legal info.

#include "rehost_ticket.h"

#include <algorithm>
#include <random>

#include "error/error.h"
#include "network/serialization.h"

namespace openage::network {

namespace {

constexpr size_t REHOST_PAYLOAD_SIZE = 64;

bool valid_ticket(const RehostTicket &ticket) {
	return std::any_of(ticket.begin(), ticket.end(), [](uint8_t byte) {
		return byte != 0;
	});
}

template<typename T>
std::vector<uint8_t> encode_ticket_message(const T &message) {
	if (message.original_lobby_revision == 0 or message.room_id == 0
	    or message.authority_epoch <= 1 or message.original_player_id < 2
	    or message.original_player_id > MAX_STATE_PLAYERS
	    or not valid_ticket(message.ticket)) {
		throw Error{MSG(err) << "Invalid FVD Hunter handoff credential"};
	}
	std::vector<uint8_t> data{};
	data.reserve(REHOST_PAYLOAD_SIZE);
	append_u64(data, message.original_lobby_revision);
	append_u64(data, message.room_id);
	append_u64(data, message.authority_epoch);
	append_u64(data, message.original_player_id);
	data.insert(data.end(), message.ticket.begin(), message.ticket.end());
	return data;
}

template<typename T>
T decode_ticket_message(const std::vector<uint8_t> &bytes) {
	if (bytes.size() != REHOST_PAYLOAD_SIZE) {
		throw Error{MSG(err) << "FVD Hunter handoff proof must be 64 bytes"};
	}
	T decoded{};
	decoded.original_lobby_revision = read_u64(bytes.data());
	decoded.room_id = read_u64(bytes.data() + 8);
	decoded.authority_epoch = read_u64(bytes.data() + 16);
	decoded.original_player_id = read_u64(bytes.data() + 24);
	std::copy(bytes.begin() + 32, bytes.end(), decoded.ticket.begin());
	(void) encode_ticket_message(decoded);
	return decoded;
}

bool same_ticket(const RehostTicket &a, const RehostTicket &b) {
	// Compare all bytes, rather than early-exiting on the first difference.
	uint8_t difference = 0;
	for (size_t i = 0; i < a.size(); ++i) difference |= a[i] ^ b[i];
	return difference == 0;
}

RehostTicket generate_ticket() {
	std::random_device entropy{};
	RehostTicket ticket{};
	for (auto &byte : ticket) byte = static_cast<uint8_t>(entropy());
	ticket[0] |= 1;
	return ticket;
}

} // namespace

std::vector<uint8_t> encode_rehost_offer(const RehostOffer &offer) {
	return encode_ticket_message(offer);
}

RehostOffer decode_rehost_offer(const std::vector<uint8_t> &bytes) {
	return decode_ticket_message<RehostOffer>(bytes);
}

std::vector<uint8_t> encode_rehost_proof(const RehostProof &proof) {
	return encode_ticket_message(proof);
}

RehostProof decode_rehost_proof(const std::vector<uint8_t> &bytes) {
	return decode_ticket_message<RehostProof>(bytes);
}

RehostProof make_rehost_proof(const RehostOffer &offer) {
	return {offer.original_lobby_revision, offer.room_id, offer.authority_epoch,
	        offer.original_player_id, offer.ticket};
}

RehostTicketBook::RehostTicketBook(
	const StartMatch &original, uint64_t room_id, uint64_t successor_epoch) :
	original_revision{original.lobby_revision},
	target_room{room_id},
	epoch{successor_epoch} {
	validate_start_match(original);
	if (room_id == 0 or successor_epoch <= 1) {
		throw Error{MSG(err) << "FVD rehost requires a room and newer authority epoch"};
	}
	for (const auto &entry : original.assignments) {
		if (entry.player_id == 1) continue;
		auto ticket = generate_ticket();
		// No two Builder players may share the same admission credential.
		while (std::any_of(this->tickets.begin(), this->tickets.end(),
		                   [&ticket](const auto &existing) {
			                   return same_ticket(existing.second, ticket);
		                   })) {
			ticket = generate_ticket();
		}
		this->tickets.emplace(entry.player_id, ticket);
	}
}

RehostOffer RehostTicketBook::offer_for(gamestate::player_id_t player) const {
	auto found = this->tickets.find(player);
	if (found == this->tickets.end()) {
		throw Error{MSG(err) << "No Hunter handoff ticket for this player"};
	}
	return {this->original_revision, this->target_room, this->epoch,
	        player, found->second};
}

bool RehostTicketBook::redeem(const RehostProof &proof, SessionIdentity accepted_session) {
	auto found = this->tickets.find(proof.original_player_id);
	if (found == this->tickets.end() or accepted_session.session_id == 0
	    or accepted_session.player_id < 2
	    or accepted_session.player_id > MAX_STATE_PLAYERS
	    or proof.original_lobby_revision != this->original_revision
	    or proof.room_id != this->target_room or proof.authority_epoch != this->epoch
	    or this->accepted_sessions.contains(proof.original_player_id)
	    or this->participant_slots.contains(accepted_session.player_id)) {
		return false;
	}
	if (not same_ticket(found->second, proof.ticket)) return false;
	for (const auto &[player, session_id] : this->accepted_sessions) {
		if (session_id == accepted_session.session_id) return false;
	}
	this->accepted_sessions.emplace(proof.original_player_id, accepted_session.session_id);
	this->participant_slots.emplace(accepted_session.player_id, proof.original_player_id);
	return true;
}

bool RehostTicketBook::has_redeemed(gamestate::player_id_t player) const {
	return this->accepted_sessions.contains(player);
}

bool RehostTicketBook::complete() const {
	return this->accepted_sessions.size() == this->tickets.size();
}

size_t RehostTicketBook::expected_guests() const {
	return this->tickets.size();
}

const std::map<uint64_t, gamestate::player_id_t> &
RehostTicketBook::new_participant_slots() const {
	return this->participant_slots;
}

} // namespace openage::network
