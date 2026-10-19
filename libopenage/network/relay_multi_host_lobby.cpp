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
	const auto payload = encode_participant_snapshot(this->lobby.snapshot());
	for (auto &peer : this->channels_by_slot) {
		if (peer.admitted) {
			peer.socket->send_frame(peer.session->make_frame(
				MessageType::PARTICIPANT_SNAPSHOT, payload));
		}
	}
}

size_t RelayMultiHostLobby::poll_admissions(const AssetDigest &digest) {
	if (this->started) return 0;
	size_t newly_joined{0};
	bool roster_changed = false;
	for (auto &peer : this->channels_by_slot) {
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
			this->lobby.join(peer.participant_id, std::move(guest_name));
			peer.session = std::make_unique<ConnectionSession>(std::move(*session));
			peer.admitted = true;
			roster_changed = true;
			++newly_joined;
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
	// All guests receive the same assignment over their own accepted
	// session. Never combine per-connection sequence numbers.
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
	return assignment;
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
