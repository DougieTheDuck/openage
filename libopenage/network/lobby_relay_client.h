// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "network/messages.h"
#include "network/transport.h"

namespace openage::network {

class ReplicationTracker;

/**
 * One pre-provisioned relay pair (host + one guest). The relay's credentials
 * come from the separate relay provisioning command; this is not matchmaking.
 */
struct RelayLobbyConfig {
	std::string address{"127.0.0.1"};
	uint16_t port{32456};
	uint64_t match_id{0};
	uint64_t pair_id{0};
	std::string token_hex{};
	std::string name{};
	bool hosting{false};
};

struct RelayLobbyStatus {
	std::string phase{"menu"};
	std::string message{"Enter the relay details provisioned by your relay server."};
	std::string participants{};
	bool can_start{false};
	bool connected{false};
	gamestate::player_id_t local_player_id{0};
	gamestate::player_id_t remote_player_id{0};
	uint64_t snapshot_revision{0};
};

/**
 * Background relay transport for the pre-match screen. Only the worker
 * touches its ConnectionSession, preserving sequence ordering.
 *
 * Match start rebinds the authenticated player slot and exchanges the
 * authoritative initial snapshot with a versioned acknowledgement. Continuous
 * gameplay replication, input commands and client rendering are separate work
 * and the match clock stays paused until those paths are connected.
 */
class RelayLobbyClient {
public:
	RelayLobbyClient() = default;
	~RelayLobbyClient();
	RelayLobbyClient(const RelayLobbyClient &) = delete;
	RelayLobbyClient &operator=(const RelayLobbyClient &) = delete;

	void connect(RelayLobbyConfig settings);
	void request_start();

	/** Supply a snapshot captured on the host simulation thread. */
	void provide_host_snapshot(WorldState snapshot);

	/** Guest's validated authoritative bootstrap state (once received). */
	std::shared_ptr<ReplicationTracker> get_replication() const;

	RelayLobbyStatus status() const;
	void close();

private:
	void run(RelayLobbyConfig settings);
	void publish(RelayLobbyStatus next);

	mutable std::mutex mutex;
	RelayLobbyStatus current{};
	std::shared_ptr<TcpConnection> connection{};
	std::optional<WorldState> pending_host_snapshot{};
	std::shared_ptr<ReplicationTracker> guest_replication{};
	bool hosting{false};
	std::thread worker{};
	std::atomic<bool> stopping{false};
	std::atomic<bool> start_requested{false};
};

} // namespace openage::network
