// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "coord/phys.h"
#include "gamestate/types.h"
#include "network/protocol.h"
#include "time/time.h"

namespace openage::network {

/** Largest entity selection accepted in one network command. */
constexpr uint32_t MAX_COMMAND_ENTITIES = 1024;

/** Largest position list accepted in one state message. */
constexpr uint32_t MAX_STATE_POSITIONS = (MAX_PAYLOAD_SIZE - 28) / 32;

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
struct EntityPosition {
	gamestate::entity_id_t entity_id{0};
	coord::phys3 position{0, 0, 0};
};

/** Position subset carried by a world snapshot or delta. */
struct WorldState {
	uint64_t base_revision{0};
	uint64_t world_revision{0};
	time::time_t simulation_time{};
	std::vector<EntityPosition> positions{};
};

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

} // namespace openage::network
