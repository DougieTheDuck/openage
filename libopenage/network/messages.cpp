// Copyright 2026 the openage authors. See copying.md for legal info.

#include "messages.h"

#include <algorithm>
#include <limits>

#include "error/error.h"
#include "network/serialization.h"

namespace openage::network {

namespace {

constexpr size_t COMMAND_PREFIX_SIZE = 16;
constexpr size_t POSITION_SIZE = 32;

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

std::vector<uint8_t> encode_world_state(const WorldState &state) {
	if (state.positions.size() > MAX_STATE_POSITIONS) {
		throw Error{MSG(err) << "FVD state contains more than "
		                     << MAX_STATE_POSITIONS << " positions"};
	}

	std::vector<uint8_t> output{};
	output.reserve(28 + state.positions.size() * POSITION_SIZE);
	append_u64(output, state.base_revision);
	append_u64(output, state.world_revision);
	append_i64(output, state.simulation_time.get_raw_value());
	append_u32(output, static_cast<uint32_t>(state.positions.size()));
	for (const auto &position : state.positions) {
		append_u64(output, position.entity_id);
		append_position(output, position.position);
	}
	return output;
}

WorldState decode_world_state(const std::vector<uint8_t> &payload) {
	PayloadReader reader{payload};
	WorldState state{};
	state.base_revision = reader.u64();
	state.world_revision = reader.u64();
	state.simulation_time = time::time_t::from_raw_value(reader.i64());
	auto position_count = reader.u32();
	if (position_count > MAX_STATE_POSITIONS) {
		throw Error{MSG(err) << "FVD state contains more than "
		                     << MAX_STATE_POSITIONS << " positions"};
	}
	state.positions.reserve(position_count);
	for (uint32_t i = 0; i < position_count; ++i) {
		state.positions.push_back({reader.u64(), read_position(reader)});
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
