// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_multi_host_lobby.h"

#include <algorithm>
#include <random>
#include <utility>

#include "error/error.h"
#include "network/messages.h"
#include "network/serialization.h"

namespace openage::network {

RelayMultiHostLobby::RelayMultiHostLobby(
	const std::string &address, uint16_t port,
	const RelayToken &host_token, const std::string &room_name,
	const std::string &host_name, size_t player_capacity) :
	lobby{1, host_name} {
	if (player_capacity < 2 or player_capacity > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "FVD room capacity must be between 2 and "
		                     << MAX_STATE_PLAYERS};
	}
	// Even public rooms need a secret creator capability for attaching
	// additional host sockets. A zero public guest token must NEVER be
	// sufficient to impersonate another room's host channels.
	RelayToken creator_token = host_token;
	if (std::all_of(creator_token.begin(), creator_token.end(),
	                [](uint8_t byte) { return byte == 0; })) {
		std::random_device entropy{};
		for (auto &byte : creator_token) {
			byte = static_cast<uint8_t>(entropy());
		}
		// Ensure no accidental all-zero capability even with a poor RNG.
		creator_token[0] |= 1;
	}
	this->channels_by_slot.reserve(player_capacity - 1);
	auto primary = std::make_shared<TcpConnection>(
		TcpConnection::connect(address, port));
	this->advertised_room_id = create_relay_room(
		*primary, creator_token, room_name, host_name);
	this->channels_by_slot.push_back({2, std::move(primary), nullptr, false});
	for (size_t index = 3; index <= player_capacity; ++index) {
		auto socket = std::make_shared<TcpConnection>(
			TcpConnection::connect(address, port));
		const auto reserved = attach_relay_room(
			*socket, creator_token, this->advertised_room_id);
		if (reserved != index) {
			throw Error{MSG(err) << "FVD relay returned a noncontiguous host slot"};
		}
		this->channels_by_slot.push_back({reserved, std::move(socket), nullptr, false});
	}
}

RelayMultiHostLobby::~RelayMultiHostLobby() {
	this->close();
}

uint64_t RelayMultiHostLobby::room_id() const {
	return this->advertised_room_id;
}

void RelayMultiHostLobby::broadcast_roster() {
	// A peer may disappear while a revised roster is being distributed.
	// Remove only that peer and rebroadcast the newer revision rather
	// than letting one broken socket terminate everyone else's lobby.
	bool retry{};
	do {
		retry = false;
		const auto payload = encode_participant_snapshot(this->lobby.snapshot());
		for (auto &peer : this->channels_by_slot) {
			if (not peer.admitted) continue;
			try {
				peer.socket->send_frame(peer.session->make_frame(
					MessageType::PARTICIPANT_SNAPSHOT, payload));
			}
			catch (...) {
				this->lobby.leave(peer.participant_id);
				peer.admitted = false;
				peer.session.reset();
				peer.socket->close();
				retry = true;
			}
		}
	} while (retry);
}

void RelayMultiHostLobby::prepare_hunter_rehost(
	const StartMatch &original_assignment) {
	if (this->started or this->lobby.size() != 1 or this->rehost_tickets) {
		throw Error{MSG(err) << "FVD Hunter rehost must be prepared before guest admission"};
	}
	if (player_for_participant(original_assignment,
	                           original_assignment.host_participant_id) == 1) {
		throw Error{MSG(err) << "Original Hunter host does not need a rehost"};
	}
	if (this->channels_by_slot.size() + 1 != original_assignment.assignments.size()) {
		throw Error{MSG(err) << "Hunter rehost must reserve all original Builder slots"};
	}
	this->rehost_tickets = std::make_unique<RehostTicketBook>(
		original_assignment, this->advertised_room_id);
	this->rehost_succession = std::make_unique<AuthoritySuccession>(
		original_assignment, original_assignment.host_participant_id);
	// The new Hunter is local to this process, so it doesn't consume a
	// remote relay channel. Give that local authority incarnation a
	// fresh session ID, distinct from any network guest.
	if (not this->rehost_succession->acknowledge_pregame_rehost(
		{generate_session_id(), 1}, 2)) {
		throw Error{MSG(err) << "Could not establish local Hunter rehost epoch"};
	}
}

RehostOffer RelayMultiHostLobby::rehost_offer_for(
	gamestate::player_id_t original_player_id) const {
	if (not this->rehost_tickets) {
		throw Error{MSG(err) << "No Hunter handoff is in progress"};
	}
	return this->rehost_tickets->offer_for(original_player_id);
}

StartMatch RelayMultiHostLobby::start_rehost() {
	if (not this->rehost_tickets or not this->rehost_tickets->complete()
	    or not this->rehost_succession or not this->rehost_succession->pregame_rehost_ready()
	    or this->lobby.size() != this->rehost_tickets->expected_guests() + 1) {
		throw Error{MSG(err) << "Hunter handoff requires every authenticated Builder"};
	}
	auto assignment = this->start_fixed(this->rehost_tickets->new_participant_slots());
	const auto lease = this->rehost_succession->commit_pregame_hunter_rehost();
	if (lease.epoch != 2 or lease.authority != 1) {
		throw Error{MSG(err) << "Invalid Hunter authority after successful rehost"};
	}
	return assignment;
}

size_t RelayMultiHostLobby::poll_admissions(const AssetDigest &digest) {
	if (this->started) return 0;
	size_t newly_joined{0};
	bool roster_changed = false;
	for (auto &peer : this->channels_by_slot) {
		if (this->rehost_tickets and peer.session and not peer.admitted) {
			auto frame = peer.socket->try_receive_frame();
			if (frame) {
				try {
					peer.session->validate_incoming(*frame);
					if (peer.session->get_protocol_version().minor < 4) {
						throw Error{MSG(err) << "FVD rehost proof requires protocol minor 4"};
					}
					if (frame->message_type != MessageType::AUTHORITY_REHOST_PROOF) {
						throw Error{MSG(err) << "Expected authenticated Hunter rehost proof"};
					}
					auto proof = decode_rehost_proof(frame->payload);
					if (not this->rehost_tickets->redeem(
						proof, peer.session->get_identity())) {
						throw Error{MSG(err) << "Invalid or reused Hunter rehost ticket"};
					}
					if (not this->rehost_succession->acknowledge_pregame_rehost(
						{peer.session->get_identity().session_id, proof.original_player_id}, 2)) {
						throw Error{MSG(err) << "Hunter succession rejected a duplicate new session"};
					}
					this->lobby.join(peer.participant_id, std::move(peer.accepted_name));
					peer.admitted = true;
					++newly_joined;
					roster_changed = true;
				}
				catch (const std::exception &) {
					peer.socket->close();
					peer.session.reset();
					peer.accepted_name.clear();
				}
			}
			else if (not peer.socket->peer_connected()
			         or std::chrono::steady_clock::now() >= peer.proof_deadline) {
				peer.socket->close();
				peer.session.reset();
				peer.accepted_name.clear();
			}
			continue;
		}
		if (peer.admitted) {
			if (not peer.socket->peer_connected()) {
				this->lobby.leave(peer.participant_id);
				peer.admitted = false;
				peer.session.reset();
				peer.socket->close();
				roster_changed = true;
			}
			continue;
		}
		if (not peer.socket->is_open()) continue;
		for (size_t attempt = 0; attempt < 16; ++attempt) {
			auto frame = peer.socket->try_receive_frame();
			if (not frame) break;
			if (frame->session_id != 0 or frame->sequence != 0
			    or frame->protocol_major != PROTOCOL_MAJOR
			    or frame->protocol_minor < RELAY_DIRECTORY_PROTOCOL_MINOR
			    or frame->protocol_minor > PROTOCOL_MINOR) {
				throw Error{MSG(err) << "Invalid multi-guest relay admission frame"};
			}
			if (frame->message_type == MessageType::PING and frame->payload.empty()) {
				continue;
			}
			if (frame->message_type != MessageType::RELAY_READY
			    or frame->payload.size() != 16
			    or read_u64(frame->payload.data()) != this->advertised_room_id
			    or read_u64(frame->payload.data() + 8) != peer.participant_id) {
				throw Error{MSG(err) << "Relay assigned an unexpected host-side guest slot"};
			}
			std::string guest_name{};
			auto session = host_handshake(
				*peer.socket, peer.participant_id, digest, &guest_name);
			if (not session) {
				peer.socket->close();
				break;
			}
			peer.session = std::make_unique<ConnectionSession>(std::move(*session));
			if (this->rehost_tickets) {
				// A handshake proves protocol compatibility, not ownership
				// of any OLD player slot. Admit only after a session-sequenced,
				// one-time handoff proof validates on a subsequent poll.
				peer.accepted_name = std::move(guest_name);
				peer.proof_deadline = std::chrono::steady_clock::now()
				                      + std::chrono::seconds{15};
			}
			else {
				this->lobby.join(peer.participant_id, std::move(guest_name));
				peer.admitted = true;
				roster_changed = true;
				++newly_joined;
			}
			break;
		}
	}
	if (roster_changed) this->broadcast_roster();
	return newly_joined;
}

size_t RelayMultiHostLobby::participant_count() const {
	return this->lobby.size();
}

ParticipantSnapshot RelayMultiHostLobby::roster() const {
	return this->lobby.snapshot();
}

StartMatch RelayMultiHostLobby::start() {
	if (this->rehost_tickets) {
		throw Error{MSG(err) << "Cannot redraw Hunter roles during a role-preserving rehost"};
	}
	if (this->started) {
		throw Error{MSG(err) << "FVD multi-guest lobby has already started"};
	}
	// A guest can close its TCP socket between the last admission poll
	// and Start. Never immortalize a known-disconnected participant in
	// the final immutable Hunter lottery.
	bool roster_changed = false;
	for (auto &peer : this->channels_by_slot) {
		if (peer.admitted and not peer.socket->peer_connected()) {
			this->lobby.leave(peer.participant_id);
			peer.admitted = false;
			peer.session.reset();
			peer.socket->close();
			roster_changed = true;
		}
	}
	if (roster_changed) this->broadcast_roster();
	const auto assignment = this->lobby.start();
	this->broadcast_start(assignment);
	return assignment;
}

StartMatch RelayMultiHostLobby::start_fixed(
	const std::map<uint64_t, gamestate::player_id_t> &player_slots) {
	if (this->rehost_tickets
	    and (not this->rehost_tickets->complete()
	         or this->rehost_tickets->new_participant_slots() != player_slots)) {
		throw Error{MSG(err) << "Rehost player IDs must match redeemed private tickets"};
	}
	if (this->started) {
		throw Error{MSG(err) << "Cannot rehost an already started FVD room"};
	}
	const auto before = this->lobby.snapshot();
	if (before.participants.size() < 2
	    or player_slots.size() != before.participants.size()) {
		throw Error{MSG(err) << "Hunter rehost requires all admitted participants"};
	}
	// The new lobby creator must be the Hunter, without changing any original
	// participant's assigned game player ID. Validate the entire bijection
	// before mutating HostLobby or publishing a new StartMatch.
	auto hunter = player_slots.find(before.host_participant_id);
	if (hunter == player_slots.end() or hunter->second != 1) {
		throw Error{MSG(err) << "FVD rehosted authority must be the Hunter"};
	}
	std::vector<bool> seen(before.participants.size() + 1, false);
	for (const auto &participant : before.participants) {
		auto found = player_slots.find(participant.participant_id);
		if (found == player_slots.end() or found->second < 1
		    or found->second >= seen.size() or seen[found->second]) {
			throw Error{MSG(err) << "Duplicate or unknown FVD rehost player slot"};
		}
		seen[found->second] = true;
	}
	// The old match already conducted the lottery; this placeholder only
	// advances HostLobby's roster revision and freezes further admission.
	auto assignment = this->lobby.start([](uint64_t) { return uint64_t{0}; });
	for (auto &entry : assignment.assignments) {
		entry.player_id = player_slots.at(entry.participant_id);
		entry.role = entry.player_id == 1 ? PlayerRole::HUNTER : PlayerRole::BUILDER;
	}
	validate_start_match_against_roster(before, assignment);
	this->broadcast_start(assignment);
	return assignment;
}

void RelayMultiHostLobby::broadcast_start(const StartMatch &assignment) {
	// Every session has its own sequence counters. No new Hunter draw is
	// permitted when control transfers before the first simulation frame.
	const auto payload = encode_start_match(assignment);
	for (auto &peer : this->channels_by_slot) {
		if (not peer.admitted) continue;
		peer.socket->send_frame(peer.session->make_frame(
			MessageType::START_MATCH, payload));
		peer.session->bind_match_player(assignment, peer.participant_id);
	}
	this->started = true;
	for (auto &peer : this->channels_by_slot) {
		if (not peer.admitted) peer.socket->close();
	}
}

const std::vector<RelayMultiHostLobby::Peer> &
RelayMultiHostLobby::channels() const {
	return this->channels_by_slot;
}

void RelayMultiHostLobby::close() {
	for (auto &peer : this->channels_by_slot) {
		if (peer.socket) peer.socket->close();
	}
}

} // namespace openage::network
