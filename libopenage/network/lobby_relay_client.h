// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "network/transport.h"

namespace openage::network {

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
};

/**
 * Background relay transport for the pre-match screen. Only the worker
 * touches its ConnectionSession, preserving sequence ordering.
 *
 * The match-level command/replication handoff is not implemented here;
 * START_MATCH intentionally leaves the game paused until that exists.
 */
class RelayLobbyClient {
public:
	RelayLobbyClient() = default;
	~RelayLobbyClient();
	RelayLobbyClient(const RelayLobbyClient &) = delete;
	RelayLobbyClient &operator=(const RelayLobbyClient &) = delete;

	void connect(RelayLobbyConfig settings);
	void request_start();
	RelayLobbyStatus status() const;
	void close();

private:
	void run(RelayLobbyConfig settings);
	void publish(RelayLobbyStatus next);

	mutable std::mutex mutex;
	RelayLobbyStatus current{};
	std::shared_ptr<TcpConnection> connection{};
	std::thread worker{};
	std::atomic<bool> stopping{false};
	std::atomic<bool> start_requested{false};
};

} // namespace openage::network
