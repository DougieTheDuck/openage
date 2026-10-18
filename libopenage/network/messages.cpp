// Copyright 2026 the openage authors. See copying.md for legal info.

#include "messages.h"

#include <algorithm>
#include <limits>

#include "error/error.h"
#include "network/serialization.h"
#include "network/lobby.h"

namespace openage::network {

namespace {

constexpr size_t COMMAND_PREFIX_SIZE = 16;
constexpr uint16_t PLAYER_CONNECTED = 1 << 0;
constexpr uint16_t ENTITY_HAS_HEALTH = 1 << 0;
constexpr uint16_t ENTITY_HAS_TOWER_KILLS = 1 << 1;
constexpr uint16_t ATTACK_HAS_TARGET = 1 << 0;
constexpr uint16_t ATTACK_REACHED_DESTINATION = 1 << 1;
constexpr uint16_t ATTACK_FOCUS_TARGET = 1 << 2;
class PayloadReader {
public:
	explicit PayloadReader(const std::vector<uint8_t> &payload) :
		payload{payload},
		offset{0} {}

	uint16_t u16() {
		this->require(sizeof(uint16_t));
		auto value = read_u16(this->payload.data() + this->offset);
		this->offset += sizeof(uint16_t);
		return value;
	}

	uint32_t u32() {
		this->require(sizeof(uint32_t));
		auto value = read_u32(this->payload.data() + this->offset);
		this->offset += sizeof(uint32_t);
		return value;
	}

	uint64_t u64() {
		this->require(sizeof(uint64_t));
		auto value = read_u64(this->payload.data() + this->offset);
		this->offset += sizeof(uint64_t);
		return value;
	}

	int64_t i64() {
		this->require(sizeof(int64_t));
		auto value = read_i64(this->payload.data() + this->offset);
		this->offset += sizeof(int64_t);
		return value;
	}

	std::string string(uint32_t maximum_size) {
		auto size = this->u32();
		if (size > maximum_size) {
			throw Error{MSG(err) << "FVD network string exceeds " << maximum_size << " bytes"};
		}
		this->require(size);
		std::string value{reinterpret_cast<const char *>(this->payload.data() + this->offset), size};
		this->offset += size;
		return value;
	}

	AssetDigest asset_digest() {
		this->require(ASSET_DIGEST_SIZE);
		AssetDigest value{};
		std::copy_n(this->payload.data() + this->offset,
		            ASSET_DIGEST_SIZE,
		            value.begin());
		this->offset += ASSET_DIGEST_SIZE;
		return value;
	}

	void finish() const {
		if (this->offset != this->payload.size()) {
			throw Error{MSG(err) << "Unexpected trailing bytes in FVD network payload"};
		}
	}

private:
	void require(size_t size) const {
		if (size > this->payload.size() - this->offset) {
			throw Error{MSG(err) << "Truncated FVD network payload"};
		}
	}

	const std::vector<uint8_t> &payload;
	size_t offset;
};

bool is_known_command_kind(uint16_t value) {
	return value >= static_cast<uint16_t>(CommandKind::MOVE)
	    && value <= static_cast<uint16_t>(CommandKind::BUILD_TOWER);
}

bool is_known_command_status(uint16_t value) {
	return value >= static_cast<uint16_t>(CommandStatus::ACCEPTED)
	    && value <= static_cast<uint16_t>(CommandStatus::REJECTED);
}

bool is_known_player_role(uint16_t value) {
	return value >= static_cast<uint16_t>(PlayerRole::HUNTER)
	    && value <= static_cast<uint16_t>(PlayerRole::BUILDER);
}

bool is_known_construction_phase(uint16_t value) {
	return value >= static_cast<uint16_t>(ConstructionPhase::PLANNED)
	    && value <= static_cast<uint16_t>(ConstructionPhase::UNDER_CONSTRUCTION);
}

void append_position(std::vector<uint8_t> &output, const coord::phys3 &position) {
	append_i64(output, position.ne.get_raw_value());
	append_i64(output, position.se.get_raw_value());
	append_i64(output, position.up.get_raw_value());
}

coord::phys3 read_position(PayloadReader &reader) {
	return {
		coord::phys_t::from_raw_value(reader.i64()),
		coord::phys_t::from_raw_value(reader.i64()),
		coord::phys_t::from_raw_value(reader.i64()),
	};
}

void append_tile(std::vector<uint8_t> &output, const coord::tile &tile) {
	append_i64(output, tile.ne);
	append_i64(output, tile.se);
}

coord::tile read_tile(PayloadReader &reader) {
	return {reader.i64(), reader.i64()};
}

void append_string(std::vector<uint8_t> &output,
	                 const std::string &value,
	                 uint32_t maximum_size) {
	if (value.size() > maximum_size
	    || value.size() > std::numeric_limits<uint32_t>::max()) {
		throw Error{MSG(err) << "FVD network string exceeds " << maximum_size << " bytes"};
	}
	append_u32(output, static_cast<uint32_t>(value.size()));
	output.insert(output.end(), value.begin(), value.end());
}

} // namespace

bool is_known_disconnect_reason(DisconnectReason reason) {
	auto value = static_cast<uint16_t>(reason);
	return value >= static_cast<uint16_t>(DisconnectReason::CONNECTION_LOST)
	    && value <= static_cast<uint16_t>(DisconnectReason::HOST_SHUTDOWN);
}

std::vector<uint8_t> encode_client_hello(const ClientHello &hello) {
	if (hello.participant_name.empty()) {
		throw Error{MSG(err) << "FVD participant name must not be empty"};
	}
	if (hello.build_version.empty()) {
		throw Error{MSG(err) << "FVD build version must not be empty"};
	}

	std::vector<uint8_t> output{};
	append_u16(output, hello.minimum_protocol_minor);
	append_u16(output, 0);
	append_string(output, hello.participant_name, MAX_PARTICIPANT_NAME_SIZE);
	append_string(output, hello.build_version, MAX_BUILD_VERSION_SIZE);
	output.insert(output.end(), hello.asset_digest.begin(), hello.asset_digest.end());
	return output;
}

ClientHello decode_client_hello(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	ClientHello hello{};
	hello.minimum_protocol_minor = reader.u16();
	if (reader.u16() != 0) {
		throw Error{MSG(err) << "Unsupported FVD client hello flags"};
	}
	hello.participant_name = reader.string(MAX_PARTICIPANT_NAME_SIZE);
	if (hello.participant_name.empty()) {
		throw Error{MSG(err) << "FVD participant name must not be empty"};
	}
	hello.build_version = reader.string(MAX_BUILD_VERSION_SIZE);
	if (hello.build_version.empty()) {
		throw Error{MSG(err) << "FVD build version must not be empty"};
	}
	hello.asset_digest = reader.asset_digest();
	reader.finish();
	return hello;
}

std::vector<uint8_t> encode_server_accept(const ServerAccept &accept) {
	if (accept.session_id == 0) {
		throw Error{MSG(err) << "Accepted FVD session ID must be nonzero"};
	}
	if (accept.player_id == 0) {
		throw Error{MSG(err) << "Accepted FVD player ID must be nonzero"};
	}

	std::vector<uint8_t> output{};
	append_u64(output, accept.session_id);
	append_u64(output, accept.player_id);
	return output;
}

ServerAccept decode_server_accept(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	ServerAccept accept{};
	accept.session_id = reader.u64();
	accept.player_id = reader.u64();
	reader.finish();
	if (accept.session_id == 0) {
		throw Error{MSG(err) << "Accepted FVD session ID must be nonzero"};
	}
	if (accept.player_id == 0) {
		throw Error{MSG(err) << "Accepted FVD player ID must be nonzero"};
	}
	return accept;
}

std::vector<uint8_t> encode_server_reject(const ServerReject &reject) {
	if (reject.reason.empty()) {
		throw Error{MSG(err) << "FVD handshake rejection reason must not be empty"};
	}

	std::vector<uint8_t> output{};
	append_string(output, reject.reason, MAX_HANDSHAKE_REJECTION_SIZE);
	return output;
}

ServerReject decode_server_reject(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	ServerReject reject{};
	reject.reason = reader.string(MAX_HANDSHAKE_REJECTION_SIZE);
	reader.finish();
	if (reject.reason.empty()) {
		throw Error{MSG(err) << "FVD handshake rejection reason must not be empty"};
	}
	return reject;
}

std::vector<uint8_t> encode_match_ready(const MatchReady &ready) {
	if (ready.player_id == 0 or ready.player_id > MAX_STATE_PLAYERS
	    or ready.snapshot_revision == 0) {
		throw Error{MSG(err) << "FVD ready acknowledgement requires a player and revision"};
	}
	std::vector<uint8_t> output{};
	append_u64(output, ready.player_id);
	append_u64(output, ready.snapshot_revision);
	return output;
}

MatchReady decode_match_ready(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	MatchReady ready{};
	ready.player_id = reader.u64();
	ready.snapshot_revision = reader.u64();
	reader.finish();
	if (ready.player_id == 0 or ready.player_id > MAX_STATE_PLAYERS
	    or ready.snapshot_revision == 0) {
		throw Error{MSG(err) << "Invalid FVD match-ready acknowledgement"};
	}
	return ready;
}

std::vector<uint8_t> encode_participant_snapshot(const ParticipantSnapshot &snapshot) {
	validate_participant_snapshot(snapshot);
	std::vector<uint8_t> bytes;
	append_u64(bytes, snapshot.revision);
	append_u64(bytes, snapshot.host_participant_id);
	append_u16(bytes, static_cast<uint16_t>(snapshot.participants.size()));
	append_u16(bytes, 0);
	for (const auto &p : snapshot.participants) {
		append_u64(bytes, p.participant_id);
		append_u16(bytes, p.is_host ? 1 : 0);
		append_u16(bytes, 0);
		append_string(bytes, p.name, MAX_PARTICIPANT_NAME_SIZE);
	}
	return bytes;
}

ParticipantSnapshot decode_participant_snapshot(const std::vector<uint8_t> &bytes) {
	PayloadReader reader{bytes};
	ParticipantSnapshot snapshot{};
	snapshot.revision = reader.u64();
	snapshot.host_participant_id = reader.u64();
	const auto count = reader.u16();
	if (reader.u16() != 0 or count == 0 or count > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "Invalid FVD participant snapshot flags or size"};
	}
	snapshot.participants.reserve(count);
	for (uint16_t i = 0; i < count; ++i) {
		const auto id = reader.u64();
		const auto flags = reader.u16();
		if ((flags & ~uint16_t{1}) != 0 or reader.u16() != 0) {
			throw Error{MSG(err) << "Unsupported FVD participant flags"};
		}
		snapshot.participants.push_back({
			id, reader.string(MAX_PARTICIPANT_NAME_SIZE), (flags & 1) != 0});
	}
	reader.finish();
	validate_participant_snapshot(snapshot);
	return snapshot;
}

std::vector<uint8_t> encode_start_match(const StartMatch &start) {
	validate_start_match(start);
	std::vector<uint8_t> bytes;
	append_u64(bytes, start.lobby_revision);
	append_u64(bytes, start.host_participant_id);
	append_u16(bytes, static_cast<uint16_t>(start.assignments.size()));
	append_u16(bytes, 0);
	for (const auto &assignment : start.assignments) {
		append_u64(bytes, assignment.participant_id);
		append_u64(bytes, assignment.player_id);
		append_u16(bytes, static_cast<uint16_t>(assignment.role));
		append_u16(bytes, 0);
	}
	return bytes;
}

StartMatch decode_start_match(const std::vector<uint8_t> &bytes) {
	PayloadReader reader{bytes};
	StartMatch start{};
	start.lobby_revision = reader.u64();
	start.host_participant_id = reader.u64();
	const auto count = reader.u16();
	if (reader.u16() != 0 or count < 2 or count > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "Invalid FVD start-match flags or participant count"};
	}
	start.assignments.reserve(count);
	for (uint16_t i = 0; i < count; ++i) {
		auto participant_id = reader.u64();
		auto player_id = reader.u64();
		const auto role = reader.u16();
		if (reader.u16() != 0 or not is_known_player_role(role)) {
			throw Error{MSG(err) << "Unknown FVD start-match role or flags"};
		}
		start.assignments.push_back({
			participant_id, player_id, static_cast<PlayerRole>(role)});
	}
	reader.finish();
	validate_start_match(start);
	return start;
}

std::vector<uint8_t> encode_command_intent(const CommandIntent &intent) {
	if (intent.entity_ids.size() > MAX_COMMAND_ENTITIES) {
		throw Error{MSG(err) << "FVD command references more than "
		                     << MAX_COMMAND_ENTITIES << " entities"};
	}
	if (not is_known_command_kind(static_cast<uint16_t>(intent.kind))) {
		throw Error{MSG(err) << "Unknown FVD command kind "
		                     << static_cast<uint16_t>(intent.kind)};
	}

	std::vector<uint8_t> output{};
	output.reserve(COMMAND_PREFIX_SIZE + intent.entity_ids.size() * sizeof(uint64_t) + 24);
	append_u64(output, intent.command_id);
	append_u16(output, static_cast<uint16_t>(intent.kind));
	append_u16(output, 0);
	append_u32(output, static_cast<uint32_t>(intent.entity_ids.size()));
	for (auto entity_id : intent.entity_ids) {
		append_u64(output, entity_id);
	}
	append_position(output, intent.target);
	return output;
}

CommandIntent decode_command_intent(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	CommandIntent intent{};
	intent.command_id = reader.u64();
	auto kind = reader.u16();
	if (not is_known_command_kind(kind)) {
		throw Error{MSG(err) << "Unknown FVD command kind " << kind};
	}
	intent.kind = static_cast<CommandKind>(kind);
	if (reader.u16() != 0) {
		throw Error{MSG(err) << "Unsupported FVD command payload flags"};
	}
	auto entity_count = reader.u32();
	if (entity_count > MAX_COMMAND_ENTITIES) {
		throw Error{MSG(err) << "FVD command references more than "
		                     << MAX_COMMAND_ENTITIES << " entities"};
	}
	intent.entity_ids.reserve(entity_count);
	for (uint32_t i = 0; i < entity_count; ++i) {
		intent.entity_ids.push_back(reader.u64());
	}
	intent.target = read_position(reader);
	reader.finish();
	return intent;
}

std::vector<uint8_t> encode_command_result(const CommandResult &result) {
	if (not is_known_command_status(static_cast<uint16_t>(result.status))) {
		throw Error{MSG(err) << "Unknown FVD command status "
		                     << static_cast<uint16_t>(result.status)};
	}

	std::vector<uint8_t> output{};
	append_u64(output, result.command_id);
	append_u16(output, static_cast<uint16_t>(result.status));
	append_u16(output, 0);
	append_u64(output, result.world_revision);
	append_i64(output, result.simulation_time.get_raw_value());
	append_string(output, result.reason, MAX_REJECTION_REASON_SIZE);
	return output;
}

CommandResult decode_command_result(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	CommandResult result{};
	result.command_id = reader.u64();
	auto status = reader.u16();
	if (not is_known_command_status(status)) {
		throw Error{MSG(err) << "Unknown FVD command status " << status};
	}
	result.status = static_cast<CommandStatus>(status);
	if (reader.u16() != 0) {
		throw Error{MSG(err) << "Unsupported FVD command result flags"};
	}
	result.world_revision = reader.u64();
	result.simulation_time = time::time_t::from_raw_value(reader.i64());
	result.reason = reader.string(MAX_REJECTION_REASON_SIZE);
	reader.finish();
	return result;
}

std::vector<uint8_t> encode_world_state(const WorldState &state, bool partial) {
	if (state.map_size == 0) {
		throw Error{MSG(err) << "FVD state requires a nonzero map size"};
	}
	if (state.local_player_id == 0) {
		throw Error{MSG(err) << "FVD state requires a local player"};
	}
	if (state.players.size() > MAX_STATE_PLAYERS
	    or state.entities.size() > MAX_STATE_ENTITIES
	    or state.reservations.size() > MAX_STATE_RESERVATIONS
	    or state.constructions.size() > MAX_STATE_CONSTRUCTIONS
	    or state.attack_orders.size() > MAX_STATE_ATTACK_ORDERS
	    or state.arrows.size() > MAX_STATE_ARROWS) {
		throw Error{MSG(err) << "FVD state collection exceeds its protocol limit"};
	}

	std::vector<uint8_t> output{};
	append_u64(output, state.base_revision);
	append_u64(output, state.world_revision);
	append_i64(output, state.simulation_time.get_raw_value());
	append_u32(output, state.map_size);
	append_u64(output, state.local_player_id);
	append_u64(output, state.local_kill_count);
	append_u32(output, static_cast<uint32_t>(state.players.size()));
	append_u32(output, static_cast<uint32_t>(state.entities.size()));
	append_u32(output, static_cast<uint32_t>(state.reservations.size()));
	append_u32(output, static_cast<uint32_t>(state.constructions.size()));
	append_u32(output, static_cast<uint32_t>(state.attack_orders.size()));
	append_u32(output, static_cast<uint32_t>(state.arrows.size()));
	bool found_local_player = false;
	for (const auto &player : state.players) {
		if (player.player_id == 0
		    or not is_known_player_role(static_cast<uint16_t>(player.role))) {
			throw Error{MSG(err) << "FVD state contains an invalid player"};
		}
		append_u64(output, player.player_id);
		append_u16(output, static_cast<uint16_t>(player.role));
		append_u16(output, player.connected ? PLAYER_CONNECTED : 0);
		found_local_player = found_local_player
		                  or player.player_id == state.local_player_id;
	}
	if (not partial and not found_local_player) {
		throw Error{MSG(err) << "FVD state local player is not playable"};
	}
	for (const auto &entity : state.entities) {
		if (entity.definition.empty()) {
			throw Error{MSG(err) << "FVD state contains an invalid entity"};
		}
		if (entity.health.has_value() != entity.maximum_health.has_value()
		    or (entity.health
		        and (*entity.maximum_health <= 0 or *entity.health < 0
		             or *entity.health > *entity.maximum_health))) {
			throw Error{MSG(err) << "FVD state contains invalid entity health"};
		}
		if (entity.tower_kills
		    and (entity.owner_id != state.local_player_id
		         or entity.definition != "fvd.data.entities.Tower")) {
			throw Error{MSG(err) << "FVD tower kills may only be sent to their owner"};
		}
		append_u64(output, entity.entity_id);
		append_u64(output, entity.owner_id);
		append_string(output, entity.definition, MAX_ENTITY_DEFINITION_SIZE);
		append_position(output, entity.position);
		append_u16(output, (entity.health ? ENTITY_HAS_HEALTH : 0)
		                   | (entity.tower_kills ? ENTITY_HAS_TOWER_KILLS : 0));
		append_u16(output, 0);
		append_i64(output, entity.health.value_or(0));
		append_i64(output, entity.maximum_health.value_or(0));
		append_u64(output, entity.tower_kills.value_or(0));
	}
	for (const auto &reservation : state.reservations) {
		append_tile(output, reservation.tile);
		append_u64(output, reservation.builder_id);
	}
	for (const auto &construction : state.constructions) {
		if (construction.order_id == 0 or construction.owner_id == 0
		    or not is_known_construction_phase(static_cast<uint16_t>(construction.phase))) {
			throw Error{MSG(err) << "FVD state contains an invalid construction order"};
		}
		append_u64(output, construction.order_id);
		append_u64(output, construction.owner_id);
		append_u64(output, construction.builder_id);
		append_tile(output, construction.target);
		append_u16(output, static_cast<uint16_t>(construction.phase));
		append_u16(output, 0);
		append_i64(output, construction.phase_end_time.get_raw_value());
		append_u64(output, construction.site_entity_id);
	}
	for (const auto &order : state.attack_orders) {
		uint16_t flags = order.target_id ? ATTACK_HAS_TARGET : 0;
		flags |= order.reached_destination ? ATTACK_REACHED_DESTINATION : 0;
		flags |= order.focus_target ? ATTACK_FOCUS_TARGET : 0;
		append_u64(output, order.horse_id);
		append_position(output, order.destination);
		append_u16(output, flags);
		append_u16(output, 0);
		append_u64(output, order.target_id.value_or(0));
	}
	for (const auto &arrow : state.arrows) {
		if (arrow.arrow_id == 0 or arrow.owner_id == 0
		    or arrow.impact_time <= arrow.launch_time) {
			throw Error{MSG(err) << "FVD state contains an invalid arrow"};
		}
		append_u64(output, arrow.arrow_id);
		append_u64(output, arrow.owner_id);
		append_u64(output, arrow.target_id);
		append_position(output, arrow.start);
		append_position(output, arrow.destination);
		append_i64(output, arrow.launch_time.get_raw_value());
		append_i64(output, arrow.impact_time.get_raw_value());
	}
	if (output.size() > MAX_PAYLOAD_SIZE) {
		throw Error{MSG(err) << "FVD state exceeds the maximum frame payload"};
	}
	return output;
}

WorldState decode_world_state(const std::vector<uint8_t> &payload, bool partial) {
	PayloadReader reader{payload};
	WorldState state{};
	state.base_revision = reader.u64();
	state.world_revision = reader.u64();
	state.simulation_time = time::time_t::from_raw_value(reader.i64());
	state.map_size = reader.u32();
	if (state.map_size == 0) {
		throw Error{MSG(err) << "FVD state requires a nonzero map size"};
	}
	state.local_player_id = reader.u64();
	state.local_kill_count = reader.u64();
	if (state.local_player_id == 0) {
		throw Error{MSG(err) << "FVD state requires a local player"};
	}
	auto player_count = reader.u32();
	auto entity_count = reader.u32();
	auto reservation_count = reader.u32();
	auto construction_count = reader.u32();
	auto attack_order_count = reader.u32();
	auto arrow_count = reader.u32();
	if (player_count > MAX_STATE_PLAYERS
	    or entity_count > MAX_STATE_ENTITIES
	    or reservation_count > MAX_STATE_RESERVATIONS
	    or construction_count > MAX_STATE_CONSTRUCTIONS
	    or attack_order_count > MAX_STATE_ATTACK_ORDERS
	    or arrow_count > MAX_STATE_ARROWS) {
		throw Error{MSG(err) << "FVD state collection exceeds its protocol limit"};
	}
	state.players.reserve(player_count);
	for (uint32_t i = 0; i < player_count; ++i) {
		auto player_id = reader.u64();
		auto role = reader.u16();
		if (player_id == 0 or not is_known_player_role(role)) {
			throw Error{MSG(err) << "FVD state contains an invalid player"};
		}
		auto flags = reader.u16();
		if ((flags & ~PLAYER_CONNECTED) != 0) {
			throw Error{MSG(err) << "Unsupported FVD player state flags"};
		}
		state.players.push_back({
			player_id,
			static_cast<PlayerRole>(role),
			(flags & PLAYER_CONNECTED) != 0,
		});
	}
	if (not partial and std::none_of(state.players.begin(), state.players.end(), [&](const auto &player) {
		return player.player_id == state.local_player_id;
	})) {
		throw Error{MSG(err) << "FVD state local player is not playable"};
	}
	state.entities.reserve(entity_count);
	for (uint32_t i = 0; i < entity_count; ++i) {
		auto entity_id = reader.u64();
		auto owner_id = reader.u64();
		auto definition = reader.string(MAX_ENTITY_DEFINITION_SIZE);
		if (definition.empty()) {
			throw Error{MSG(err) << "FVD state contains an invalid entity"};
		}
		auto position = read_position(reader);
		auto flags = reader.u16();
		if ((flags & ~(ENTITY_HAS_HEALTH | ENTITY_HAS_TOWER_KILLS)) != 0
		    or reader.u16() != 0) {
			throw Error{MSG(err) << "Unsupported FVD entity state flags"};
		}
		auto health = reader.i64();
		auto maximum_health = reader.i64();
		auto tower_kills = reader.u64();
		EntityState entity{entity_id, owner_id, std::move(definition), position};
		if ((flags & ENTITY_HAS_TOWER_KILLS) != 0) {
			if (owner_id != state.local_player_id
			    or entity.definition != "fvd.data.entities.Tower") {
				throw Error{MSG(err) << "FVD tower kills belong only to the recipient"};
			}
			entity.tower_kills = tower_kills;
		}
		else if (tower_kills != 0) {
			throw Error{MSG(err) << "FVD entity tower kills present without flag"};
		}
		if ((flags & ENTITY_HAS_HEALTH) != 0) {
			if (maximum_health <= 0 or health < 0 or health > maximum_health) {
				throw Error{MSG(err) << "FVD state contains invalid entity health"};
			}
			entity.health = health;
			entity.maximum_health = maximum_health;
		}
		else if (health != 0 or maximum_health != 0) {
			throw Error{MSG(err) << "FVD entity health is present without its flag"};
		}
		state.entities.push_back(std::move(entity));
	}
	state.reservations.reserve(reservation_count);
	for (uint32_t i = 0; i < reservation_count; ++i) {
		auto tile = read_tile(reader);
		auto builder_id = reader.u64();
		state.reservations.push_back({tile, builder_id});
	}
	state.constructions.reserve(construction_count);
	for (uint32_t i = 0; i < construction_count; ++i) {
		ConstructionState construction{};
		construction.order_id = reader.u64();
		construction.owner_id = reader.u64();
		construction.builder_id = reader.u64();
		construction.target = read_tile(reader);
		auto phase = reader.u16();
		if (reader.u16() != 0) {
			throw Error{MSG(err) << "Unsupported FVD construction state flags"};
		}
		construction.phase_end_time = time::time_t::from_raw_value(reader.i64());
		construction.site_entity_id = reader.u64();
		if (construction.order_id == 0 or construction.owner_id == 0
		    or not is_known_construction_phase(phase)) {
			throw Error{MSG(err) << "FVD state contains an invalid construction order"};
		}
		construction.phase = static_cast<ConstructionPhase>(phase);
		state.constructions.push_back(construction);
	}
	state.attack_orders.reserve(attack_order_count);
	for (uint32_t i = 0; i < attack_order_count; ++i) {
		AttackOrderState order{};
		order.horse_id = reader.u64();
		order.destination = read_position(reader);
		auto flags = reader.u16();
		if ((flags & ~(ATTACK_HAS_TARGET
		              | ATTACK_REACHED_DESTINATION
		              | ATTACK_FOCUS_TARGET)) != 0
		    or reader.u16() != 0) {
			throw Error{MSG(err) << "Unsupported FVD attack order flags"};
		}
		auto target_id = reader.u64();
		if ((flags & ATTACK_HAS_TARGET) != 0) {
			order.target_id = target_id;
		}
		else if (target_id != 0) {
			throw Error{MSG(err) << "FVD attack target is present without its flag"};
		}
		order.reached_destination = (flags & ATTACK_REACHED_DESTINATION) != 0;
		order.focus_target = (flags & ATTACK_FOCUS_TARGET) != 0;
		state.attack_orders.push_back(order);
	}
	state.arrows.reserve(arrow_count);
	for (uint32_t i = 0; i < arrow_count; ++i) {
		ArrowState arrow{};
		arrow.arrow_id = reader.u64();
		arrow.owner_id = reader.u64();
		arrow.target_id = reader.u64();
		arrow.start = read_position(reader);
		arrow.destination = read_position(reader);
		arrow.launch_time = time::time_t::from_raw_value(reader.i64());
		arrow.impact_time = time::time_t::from_raw_value(reader.i64());
		if (arrow.arrow_id == 0 or arrow.owner_id == 0
		    or arrow.impact_time <= arrow.launch_time) {
			throw Error{MSG(err) << "FVD state contains an invalid arrow"};
		}
		state.arrows.push_back(arrow);
	}
	reader.finish();
	return state;
}

std::vector<uint8_t> encode_snapshot_request(const SnapshotRequest &request) {
	std::vector<uint8_t> output{};
	append_u64(output, request.last_applied_revision);
	return output;
}

SnapshotRequest decode_snapshot_request(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	SnapshotRequest request{reader.u64()};
	reader.finish();
	return request;
}

std::vector<uint8_t> encode_participant_disconnected(
	const ParticipantDisconnected &disconnected) {
	if (disconnected.player_id == 0) {
		throw Error{MSG(err) << "Disconnected FVD participant ID must be nonzero"};
	}
	if (not is_known_disconnect_reason(disconnected.reason)) {
		throw Error{MSG(err) << "Unknown FVD disconnect reason "
		                     << static_cast<uint16_t>(disconnected.reason)};
	}

	std::vector<uint8_t> output{};
	append_u64(output, disconnected.player_id);
	append_u16(output, static_cast<uint16_t>(disconnected.reason));
	append_u16(output, 0);
	return output;
}

ParticipantDisconnected decode_participant_disconnected(
	const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	ParticipantDisconnected disconnected{};
	disconnected.player_id = reader.u64();
	auto reason = reader.u16();
	if (not is_known_disconnect_reason(static_cast<DisconnectReason>(reason))) {
		throw Error{MSG(err) << "Unknown FVD disconnect reason " << reason};
	}
	disconnected.reason = static_cast<DisconnectReason>(reason);
	if (reader.u16() != 0) {
		throw Error{MSG(err) << "Unsupported FVD participant disconnect flags"};
	}
	reader.finish();
	if (disconnected.player_id == 0) {
		throw Error{MSG(err) << "Disconnected FVD participant ID must be nonzero"};
	}
	return disconnected;
}

} // namespace openage::network
