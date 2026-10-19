// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "network/messages.h"
#include "network/command_mailbox.h"
#include "network/transport.h"

namespace openage::network {

class ReplicationTracker;

/**
 * One host or guest request to a relay-managed public room directory.
 */
struct RelayLobbyConfig {
	std::string address{"127.0.0.1"};
	uint16_t port{32456};
	uint64_t room_id{0};
	std::string room_name{"FVD Game"};
	std::string token_hex{};
	std::string name{};
	bool hosting{false};
};

struct RelayLobbyStatus {
	std::string phase{"menu"};
	std::string message{"Host a game or refresh the relay room list."};
	std::string participants{};
	bool can_start{false};
	bool connected{false};
	gamestate::player_id_t local_player_id{0};
	gamestate::player_id_t remote_player_id{0};
	uint64_t snapshot_revision{0};
	/** Number of full recovery snapshots accepted after initial bootstrap. */
	uint32_t snapshot_recoveries{0};
	/** Authenticated gameplay heartbeat replies, useful for link diagnostics. */
	uint32_t heartbeat_acknowledgements{0};
	std::string rooms{};
	uint64_t suggested_room_id{0};
	/** The same name/role assignments on every peer, established by StartMatch. */
	std::string roles{};
	/** Role assigned to this client by the host, not chosen locally. */
	std::string local_role{};
	/** Seconds remaining in the five-second role reveal. */
	uint8_t role_seconds_remaining{0};
};

/**
 * Background relay transport for the pre-match screen. Only the worker
 * touches its ConnectionSession, preserving sequence ordering.
 *
 * Match start rebinds the authenticated player slot and exchanges the
 * authoritative initial snapshot with a versioned acknowledgement. Subsequent
 * complete host captures are encoded as WORLD_DELTA packets for the guest's
 * ReplicationTracker. After the five-second reveal the host resumes its
 * simulation, and the guest sends authenticated COMMAND_INTENT frames over
 * this same relay connection. GUI rendering consumes host state instead of
 * advancing a separate guest simulation. Online gameplay currently supports
 * two peers and the basic Move, AttackMove, and BuildTower intents.
 */
class RelayLobbyClient {
public:
	RelayLobbyClient() = default;
	~RelayLobbyClient();
	RelayLobbyClient(const RelayLobbyClient &) = delete;
	RelayLobbyClient &operator=(const RelayLobbyClient &) = delete;

	void connect(RelayLobbyConfig settings);
	void refresh(RelayLobbyConfig settings);
	void request_start();

	/** Supply a snapshot captured on the host simulation thread. */
	void provide_host_snapshot(WorldState snapshot);

	/**
	 * Queue the newest complete recipient-specific authoritative world.
	 * The relay worker calculates a delta against its last transmitted state.
	 * A single pending slot prevents a slow guest from growing an unbounded
	 * queue; skipped intermediate captures are folded into the next delta.
	 */
	void provide_host_update(WorldState world);

	/** Queue one locally authored intent. Only guests send intents over TCP. */
	bool send_guest_command(CommandIntent intent);

	/** Drain remote intents paired with the immutable accepted guest identity. */
	std::vector<AuthenticatedCommand> take_host_commands();

	/** Queue one authoritative decision for delivery to the guest. */
	void send_command_result(CommandResult result);

	/** Consume command decisions (or rejections) on the guest presenter. */
	std::vector<CommandResult> take_command_results();

	/** Request a recipient-specific authoritative snapshot on the active guest. */
	bool request_recovery_snapshot();

	/** Guest's validated authoritative bootstrap state (once received). */
	std::shared_ptr<ReplicationTracker> get_replication() const;

	RelayLobbyStatus status() const;
	void close();

private:
	void run(RelayLobbyConfig settings);
	void run_refresh(RelayLobbyConfig settings);
	void publish(RelayLobbyStatus next);
	void publish_stream_revision(uint64_t revision, bool local_host);
	bool reveal_finished_locked() const;

	mutable std::mutex mutex;
	RelayLobbyStatus current{};
	std::shared_ptr<TcpConnection> connection{};
	std::optional<WorldState> pending_host_snapshot{};
	std::optional<WorldState> pending_host_update{};
	std::deque<CommandIntent> pending_guest_commands{};
	std::deque<AuthenticatedCommand> incoming_host_commands{};
	std::deque<CommandResult> pending_command_results{};
	std::deque<CommandResult> guest_command_results{};
	uint64_t next_guest_command_id{1};
	bool guest_snapshot_requested{false};
	/** Largest revision submitted, including an update in flight. */
	uint64_t last_host_offered_revision{0};
	std::shared_ptr<ReplicationTracker> guest_replication{};
	bool hosting{false};
	std::optional<std::chrono::steady_clock::time_point> roles_assigned_at{};
	std::thread worker{};
	std::atomic<bool> stopping{false};
	std::atomic<bool> start_requested{false};
};

} // namespace openage::network
