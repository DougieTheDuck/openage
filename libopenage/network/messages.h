// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coord/phys.h"
#include "coord/tile.h"
#include "gamestate/types.h"
#include "network/protocol.h"
#include "time/time.h"

namespace openage::network {

/** Byte length of the gameplay asset manifest digest. */
constexpr size_t ASSET_DIGEST_SIZE = 32;

/** Largest participant name accepted during the handshake. */
constexpr uint32_t MAX_PARTICIPANT_NAME_SIZE = 128;

/** Largest game build version accepted during the handshake. */
constexpr uint32_t MAX_BUILD_VERSION_SIZE = 128;

/** Largest displayable handshake rejection reason. */
constexpr uint32_t MAX_HANDSHAKE_REJECTION_SIZE = 4096;

/** Largest entity selection accepted in one network command. */
constexpr uint32_t MAX_COMMAND_ENTITIES = 1024;

/** Largest entity definition identifier accepted in world state. */
constexpr uint32_t MAX_ENTITY_DEFINITION_SIZE = 256;

/** Defensive collection limits for one world state payload. */
constexpr uint32_t MAX_STATE_PLAYERS = 8;
constexpr uint32_t MAX_STATE_ENTITIES = 16384;
constexpr uint32_t MAX_STATE_RESERVATIONS = 16384;
constexpr uint32_t MAX_STATE_CONSTRUCTIONS = 1024;

/** Largest displayable command rejection reason. */
constexpr uint32_t MAX_REJECTION_REASON_SIZE = 4096;

/** Commands a client may request from the authoritative host. */
enum class CommandKind : uint16_t {
	MOVE = 1,
	ATTACK_MOVE = 2,
	BUILD_TOWER = 3,
};

/** Host decision for one client command. */
enum class CommandStatus : uint16_t {
	ACCEPTED = 1,
	REJECTED = 2,
};

/** Reason an accepted participant connection ended. */
enum class DisconnectReason : uint16_t {
	CONNECTION_LOST = 1,
	LEFT_MATCH = 2,
	PROTOCOL_ERROR = 3,
	HOST_SHUTDOWN = 4,
};

/** Whether a disconnect reason is defined by this protocol version. */
bool is_known_disconnect_reason(DisconnectReason reason);

/** SHA-256 digest of gameplay-affecting modpack manifests and data. */
using AssetDigest = std::array<uint8_t, ASSET_DIGEST_SIZE>;

/** Compatibility information sent by a connecting participant. */
struct ClientHello {
	uint16_t minimum_protocol_minor{PROTOCOL_MINIMUM_COMPATIBLE_MINOR};
	std::string participant_name{};
	std::string build_version{};
	AssetDigest asset_digest{};
};

/** Identity assigned by the host to an accepted connection. */
struct ServerAccept {
	uint64_t session_id{0};
	gamestate::player_id_t player_id{0};
};

/** Displayable reason why the host refused a connection. */
struct ServerReject {
	std::string reason{};
};

/** Player-independent client intent. Ownership comes from the connection. */
struct CommandIntent {
	uint64_t command_id{0};
	CommandKind kind{CommandKind::MOVE};
	std::vector<gamestate::entity_id_t> entity_ids{};
	coord::phys3 target{0, 0, 0};
};

/** Authoritative result for one submitted command. */
struct CommandResult {
	uint64_t command_id{0};
	CommandStatus status{CommandStatus::REJECTED};
	uint64_t world_revision{0};
	time::time_t simulation_time{};
	std::string reason{};
};

/** One authoritative entity position. */
struct EntityState {
	gamestate::entity_id_t entity_id{0};
	gamestate::player_id_t owner_id{0};
	std::string definition{};
	coord::phys3 position{0, 0, 0};
};

/** FVD role assigned to one playable player. */
enum class PlayerRole : uint16_t {
	HUNTER = 1,
	BUILDER = 2,
};

/** One playable player in authoritative world state. */
struct PlayerState {
	gamestate::player_id_t player_id{0};
	PlayerRole role{PlayerRole::BUILDER};
};

/** One tile held by an unfinished construction order. */
struct TileReservation {
	coord::tile tile{0, 0};
	gamestate::entity_id_t builder_id{0};
};

/** Network-visible phase of an unfinished construction order. */
enum class ConstructionPhase : uint16_t {
	PLANNED = 1,
	UNDER_CONSTRUCTION = 2,
};

/** One authoritative unfinished tower construction order. */
struct ConstructionState {
	uint64_t order_id{0};
	gamestate::player_id_t owner_id{0};
	gamestate::entity_id_t builder_id{0};
	coord::tile target{0, 0};
	ConstructionPhase phase{ConstructionPhase::PLANNED};
	time::time_t phase_end_time{};
	gamestate::entity_id_t site_entity_id{0};
};

/** Complete authoritative FVD state carried by a snapshot or delta. */
struct WorldState {
	uint64_t base_revision{0};
	uint64_t world_revision{0};
	time::time_t simulation_time{};
	uint32_t map_size{0};
	std::vector<PlayerState> players{};
	std::vector<EntityState> entities{};
	std::vector<TileReservation> reservations{};
	std::vector<ConstructionState> constructions{};
};

/** Request for a full snapshot after a client cannot apply a world delta. */
struct SnapshotRequest {
	uint64_t last_applied_revision{0};
};

/** Authoritative notification that a participant left the match. */
struct ParticipantDisconnected {
	gamestate::player_id_t player_id{0};
	DisconnectReason reason{DisconnectReason::CONNECTION_LOST};
};

/** Encode a ClientHello payload. */
std::vector<uint8_t> encode_client_hello(const ClientHello &hello);

/** Decode and validate a ClientHello payload. */
ClientHello decode_client_hello(const std::vector<uint8_t> &payload);

/** Encode a ServerAccept payload. */
std::vector<uint8_t> encode_server_accept(const ServerAccept &accept);

/** Decode and validate a ServerAccept payload. */
ServerAccept decode_server_accept(const std::vector<uint8_t> &payload);

/** Encode a ServerReject payload. */
std::vector<uint8_t> encode_server_reject(const ServerReject &reject);

/** Decode and validate a ServerReject payload. */
ServerReject decode_server_reject(const std::vector<uint8_t> &payload);

/** Encode a CommandIntent payload. */
std::vector<uint8_t> encode_command_intent(const CommandIntent &intent);

/** Decode and validate a CommandIntent payload. */
CommandIntent decode_command_intent(const std::vector<uint8_t> &payload);

/** Encode a CommandResult payload. */
std::vector<uint8_t> encode_command_result(const CommandResult &result);

/** Decode and validate a CommandResult payload. */
CommandResult decode_command_result(const std::vector<uint8_t> &payload);

/** Encode a WorldSnapshot or WorldDelta payload. */
std::vector<uint8_t> encode_world_state(const WorldState &state);

/** Decode and validate a WorldSnapshot or WorldDelta payload. */
WorldState decode_world_state(const std::vector<uint8_t> &payload);

/** Encode a SnapshotRequest payload. */
std::vector<uint8_t> encode_snapshot_request(const SnapshotRequest &request);

/** Decode and validate a SnapshotRequest payload. */
SnapshotRequest decode_snapshot_request(const std::vector<uint8_t> &payload);

/** Encode a ParticipantDisconnected payload. */
std::vector<uint8_t> encode_participant_disconnected(
	const ParticipantDisconnected &disconnected);

/** Decode and validate a ParticipantDisconnected payload. */
ParticipantDisconnected decode_participant_disconnected(
	const std::vector<uint8_t> &payload);

} // namespace openage::network
