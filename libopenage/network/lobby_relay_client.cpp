// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby_relay_client.h"

#include <chrono>
#include <limits>
#include <utility>

#include "error/error.h"
#include "network/handshake.h"
#include "network/lobby.h"
#include "network/replication.h"
#include "network/relay.h"

namespace openage::network {

namespace {

uint8_t hex_digit(char ch) {
	if (ch >= '0' and ch <= '9') return static_cast<uint8_t>(ch - '0');
	if (ch >= 'a' and ch <= 'f') return static_cast<uint8_t>(ch - 'a' + 10);
	if (ch >= 'A' and ch <= 'F') return static_cast<uint8_t>(ch - 'A' + 10);
	throw Error{MSG(err) << "The relay token must contain hexadecimal characters"};
}

RelayToken parse_token(const std::string &hex) {
	if (hex.size() != RELAY_TOKEN_SIZE * 2) {
		throw Error{MSG(err) << "A relay token needs exactly 64 hexadecimal characters"};
	}
	RelayToken token{};
	for (size_t i = 0; i < RELAY_TOKEN_SIZE; ++i) {
		token[i] = static_cast<uint8_t>(
			hex_digit(hex[i * 2]) * 16 + hex_digit(hex[i * 2 + 1]));
	}
	return token;
}

// Temporary developer-only digest. Replace with the configured gameplay
// asset-manifest SHA-256 before opening Internet matchmaking.
AssetDigest provisional_gameplay_digest() {
	AssetDigest digest{};
	for (size_t i = 0; i < digest.size(); ++i) {
		digest[i] = static_cast<uint8_t>(i + 61);
	}
	return digest;
}

std::string local_role_name(const StartMatch &start, gamestate::player_id_t local_player) {
	for (const auto &assignment : start.assignments) {
		if (assignment.player_id == local_player) {
			return assignment.role == PlayerRole::HUNTER ? "HUNTER" : "BUILDER";
		}
	}
	throw Error{MSG(err) << "No FVD role assigned to local player"};
}

void validate_config(const RelayLobbyConfig &cfg) {
	if (cfg.address.empty() or cfg.port == 0 or cfg.match_id == 0
	    or cfg.pair_id == 0 or cfg.name.empty()
	    or cfg.name.size() > MAX_PARTICIPANT_NAME_SIZE) {
		throw Error{MSG(err) << "Relay address, port, match ID, pair ID and player name are required"};
	}
	(void) parse_token(cfg.token_hex);
}

void monitor_bootstrapped_connection(TcpConnection &connection,
                                     ConnectionSession &session,
                                     const std::atomic<bool> &stopping) {
	auto frame = connection.receive_frame();
	if (not frame) {
		if (not stopping) {
			throw Error{MSG(err) << "Relay peer disconnected after match bootstrap"};
		}
		return;
	}
	session.validate_incoming(*frame);
	throw Error{MSG(err) << "Unexpected FVD message "
	                     << static_cast<uint16_t>(frame->message_type)
	                     << " after match bootstrap"};
}

} // namespace

RelayLobbyClient::~RelayLobbyClient() {
	this->close();
}

void RelayLobbyClient::connect(RelayLobbyConfig settings) {
	this->close();
	// Validate before creating any network worker.
	validate_config(settings);
	this->stopping = false;
	this->start_requested = false;
	{
		std::lock_guard lock{this->mutex};
		this->hosting = settings.hosting;
		this->pending_host_snapshot.reset();
		this->guest_replication.reset();
		this->roles_assigned_at.reset();
	}
	this->publish({"connecting", "Connecting to provisioned relay...", {}, false, false});
	this->worker = std::thread{[this, settings = std::move(settings)]() {
		this->run(settings);
	}};
}

void RelayLobbyClient::request_start() {
	std::lock_guard lock{this->mutex};
	if (this->current.can_start and this->current.connected) {
		this->start_requested = true;
	}
}

void RelayLobbyClient::provide_host_snapshot(WorldState snapshot) {
	std::lock_guard lock{this->mutex};
	if (not this->hosting or this->current.phase != "assigned"
	    or this->current.remote_player_id == 0
	    or snapshot.local_player_id != this->current.remote_player_id
	    or snapshot.base_revision != 0 or snapshot.world_revision == 0
	    or this->pending_host_snapshot) {
		throw Error{MSG(err) << "Invalid or duplicate FVD host bootstrap snapshot"};
	}
	this->pending_host_snapshot = std::move(snapshot);
}

std::shared_ptr<ReplicationTracker> RelayLobbyClient::get_replication() const {
	std::lock_guard lock{this->mutex};
	return this->guest_replication;
}

RelayLobbyStatus RelayLobbyClient::status() const {
	std::lock_guard lock{this->mutex};
	auto result = this->current;
	if (this->roles_assigned_at
	    and (result.phase == "assigned" or result.phase == "bootstrap")) {
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - *this->roles_assigned_at);
		result.role_seconds_remaining = role_reveal_seconds_remaining(elapsed);
	}
	return result;
}

void RelayLobbyClient::publish(RelayLobbyStatus next) {
	std::lock_guard lock{this->mutex};
	if (next.phase == "assigned" and not this->roles_assigned_at) {
		// Start exactly once per lobby at the receipt/publication of START_MATCH.
		this->roles_assigned_at = std::chrono::steady_clock::now();
	}
	this->current = std::move(next);
}

void RelayLobbyClient::close() {
	this->stopping = true;
	this->start_requested = false;
	std::shared_ptr<TcpConnection> socket;
	{
		std::lock_guard lock{this->mutex};
		socket = this->connection;
	}
	if (socket) socket->close();
	if (this->worker.joinable()) this->worker.join();
	{
		std::lock_guard lock{this->mutex};
		this->connection.reset();
		this->pending_host_snapshot.reset();
		this->guest_replication.reset();
		this->roles_assigned_at.reset();
		this->hosting = false;
		this->current = {};
	}
}

void RelayLobbyClient::run(RelayLobbyConfig settings) {
	try {
		const RelayConnect admission{
			settings.match_id,
			settings.pair_id,
			settings.hosting ? RelayRole::HOST : RelayRole::GUEST,
			parse_token(settings.token_hex)};
		auto socket = std::make_shared<TcpConnection>(
			TcpConnection::connect(settings.address, settings.port));
		{
			std::lock_guard lock{this->mutex};
			this->connection = socket;
		}
		if (this->stopping) return;
		// Keep the socket visible to close(): relay admission can block
		// until the second player joins and must remain cancellable.
		authenticate_relay(*socket, admission);
		if (this->stopping) return;

		if (settings.hosting) {
			std::string guest_name{};
			auto accepted = host_handshake(
				*socket, 2, provisional_gameplay_digest(), &guest_name);
			if (not accepted) {
				throw Error{MSG(err) << "Relay guest rejected by handshake"};
			}
			HostLobby lobby{1, settings.name};
			lobby.join(2, guest_name);
			const auto roster = lobby.snapshot();
			socket->send_frame(accepted->make_frame(
				MessageType::PARTICIPANT_SNAPSHOT,
				encode_participant_snapshot(roster)));
			this->publish({"lobby", "Guest connected. Host may start the match.",
				settings.name + " (host)\n" + guest_name + " (guest)", true, true});
			while (not this->stopping and not this->start_requested) {
				std::this_thread::sleep_for(std::chrono::milliseconds{20});
			}
			if (this->stopping) return;
			auto assignment = lobby.start();
			validate_start_match_against_roster(roster, assignment);
			socket->send_frame(accepted->make_frame(
				MessageType::START_MATCH, encode_start_match(assignment)));
			// The handshake's temporary slot 2 is not the role lottery result.
			// Preserve the accepted session ID and sequence numbers.
			accepted->bind_match_player(assignment, 2);
			const auto guest_player = accepted->get_identity().player_id;
			const auto host_player = player_for_participant(assignment, 1);
			const auto roles = describe_match_roles(roster, assignment);
			const auto host_role = local_role_name(assignment, host_player);
			this->publish({"assigned",
				"Match start requested; synchronizing the initial world.",
				roster.participants[0].name + " (host)\n" + guest_name + " (guest)",
				false, true, host_player, guest_player, 0, roles, host_role});
			while (not this->stopping) {
				std::optional<WorldState> snapshot;
				{
					std::lock_guard lock{this->mutex};
					if (this->pending_host_snapshot) {
						snapshot = std::move(this->pending_host_snapshot);
						this->pending_host_snapshot.reset();
					}
				}
				if (snapshot) {
					socket->send_frame(accepted->make_frame(
						MessageType::WORLD_SNAPSHOT, encode_world_state(*snapshot)));
					auto ready_frame = socket->receive_frame();
					if (not ready_frame) {
						throw Error{MSG(err) << "Guest disconnected before initial world acknowledgement"};
					}
					accepted->validate_incoming(*ready_frame);
					if (ready_frame->message_type != MessageType::MATCH_READY) {
						throw Error{MSG(err) << "Expected initial world acknowledgement"};
					}
					const auto ready = decode_match_ready(ready_frame->payload);
					if (ready.player_id != guest_player
					    or ready.snapshot_revision != snapshot->world_revision) {
						throw Error{MSG(err) << "Guest acknowledged the wrong player or world revision"};
					}
					this->publish({"bootstrap",
						"Initial world acknowledged. Live gameplay networking is not connected yet.",
						roster.participants[0].name + " (host)\n" + guest_name + " (guest)",
						false, true, host_player, guest_player, snapshot->world_revision,
						roles, host_role});
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{20});
			}
			if (not this->stopping) {
				monitor_bootstrapped_connection(*socket, *accepted, this->stopping);
			}
		}
		else {
			auto session = client_handshake(*socket, {
				PROTOCOL_MINIMUM_COMPATIBLE_MINOR, settings.name,
				"fvd-lobby", provisional_gameplay_digest()});
			auto frame = socket->receive_frame();
			if (not frame) {
				throw Error{MSG(err) << "Relay host closed before lobby snapshot"};
			}
			session.validate_incoming(*frame);
			if (frame->message_type != MessageType::PARTICIPANT_SNAPSHOT) {
				throw Error{MSG(err) << "Expected a relay lobby roster"};
			}
			const auto roster = decode_participant_snapshot(frame->payload);
			std::string people{};
			for (const auto &person : roster.participants) {
				if (not people.empty()) people += "\n";
				people += person.name + (person.is_host ? " (host)" : " (guest)");
			}
			this->publish({"lobby", "Connected. Waiting for host to start...", people, false, true});
			frame = socket->receive_frame();
			if (not frame) {
				throw Error{MSG(err) << "Relay host closed before match start"};
			}
			session.validate_incoming(*frame);
			if (frame->message_type != MessageType::START_MATCH) {
				throw Error{MSG(err) << "Expected the match start assignment"};
			}
			const auto assigned = decode_start_match(frame->payload);
			validate_start_match_against_roster(roster, assigned);
			session.bind_match_player(assigned, 2);
			const auto guest_player = session.get_identity().player_id;
			const auto host_player = player_for_participant(assigned, 1);
			const auto roles = describe_match_roles(roster, assigned);
			const auto guest_role = local_role_name(assigned, guest_player);
			this->publish({"assigned",
				"Match start requested; waiting for authoritative initial world.",
				people, false, true, guest_player, host_player, 0, roles, guest_role});
			frame = socket->receive_frame();
			if (not frame) {
				throw Error{MSG(err) << "Host disconnected before initial world"};
			}
			session.validate_incoming(*frame);
			if (frame->message_type != MessageType::WORLD_SNAPSHOT) {
				throw Error{MSG(err) << "Expected an authoritative world snapshot"};
			}
			auto tracker = std::make_shared<ReplicationTracker>(guest_player);
			tracker->accept_snapshot(decode_world_state(frame->payload));
			const auto revision = tracker->get_revision();
			socket->send_frame(session.make_frame(
				MessageType::MATCH_READY,
				encode_match_ready({guest_player, revision})));
			{
				std::lock_guard lock{this->mutex};
				this->guest_replication = std::move(tracker);
			}
			this->publish({"bootstrap",
				"Initial world received. Live gameplay networking is not connected yet.",
				people, false, true, guest_player, host_player, revision,
				roles, guest_role});
			if (not this->stopping) {
				monitor_bootstrapped_connection(*socket, session, this->stopping);
			}
		}
	}
	catch (const std::exception &error) {
		std::shared_ptr<TcpConnection> socket;
		{
			std::lock_guard lock{this->mutex};
			socket = this->connection;
		}
		if (socket) {
			socket->close();
		}
		if (not this->stopping) {
			this->publish({"error", error.what(), {}, false, false});
		}
	}
}

} // namespace openage::network
