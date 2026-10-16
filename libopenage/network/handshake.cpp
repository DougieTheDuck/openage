// Copyright 2026 the openage authors. See copying.md for legal info.

#include "handshake.h"

#include <algorithm>
#include <mutex>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>

#include "error/error.h"
#include "network/transport.h"

namespace openage::network {

namespace {

Frame make_handshake_frame(MessageType message_type,
                           std::vector<uint8_t> payload,
                           ProtocolVersion protocol_version = {}) {
	Frame frame{};
	frame.protocol_major = protocol_version.major;
	frame.protocol_minor = protocol_version.minor;
	frame.message_type = message_type;
	frame.session_id = 0;
	frame.sequence = 0;
	frame.payload = std::move(payload);
	return frame;
}

void require_handshake_envelope(const Frame &frame) {
	if (frame.session_id != 0 or frame.sequence != 0) {
		throw Error{MSG(err) << "FVD handshake frames require zero session and sequence"};
	}
}

std::string compatibility_rejection(const Frame &frame,
                                    const ClientHello &hello,
                                    const AssetDigest &required_asset_digest) {
	if (hello.minimum_protocol_minor > frame.protocol_minor) {
		return "client protocol compatibility range is invalid";
	}
	auto negotiated_minor = std::min(frame.protocol_minor, PROTOCOL_MINOR);
	if (negotiated_minor < PROTOCOL_MINIMUM_COMPATIBLE_MINOR
	    or negotiated_minor < hello.minimum_protocol_minor) {
		return "protocol minor versions are not compatible";
	}
	if (hello.asset_digest != required_asset_digest) {
		return "gameplay asset manifest does not match host";
	}
	return {};
}

void send_rejection(TcpConnection &connection, std::string reason) {
	connection.send_frame(make_handshake_frame(
		MessageType::SERVER_REJECT,
		encode_server_reject({std::move(reason)})));
}

void require_server_reply(const Frame &frame) {
	require_handshake_envelope(frame);
	if (frame.message_type != MessageType::SERVER_ACCEPT
	    and frame.message_type != MessageType::SERVER_REJECT) {
		throw Error{MSG(err) << "Expected FVD server handshake reply, received message "
		                     << static_cast<uint16_t>(frame.message_type)};
	}
}

} // namespace

Frame make_client_hello_frame(const ClientHello &hello) {
	if (hello.minimum_protocol_minor > PROTOCOL_MINOR) {
		throw Error{MSG(err) << "FVD client minimum protocol minor exceeds current version"};
	}
	return make_handshake_frame(MessageType::CLIENT_HELLO,
	                            encode_client_hello(hello));
}

uint64_t generate_session_id() {
	static std::mutex generator_mutex{};
	static std::mt19937_64 generator{[]() {
		std::random_device random{};
		std::seed_seq seed{
			random(), random(), random(), random(),
			random(), random(), random(), random(),
		};
		return std::mt19937_64{seed};
	}()};
	static std::unordered_set<uint64_t> issued_ids{};

	std::scoped_lock lock{generator_mutex};
	uint64_t session_id{};
	do {
		session_id = generator();
	} while (session_id == 0 or not issued_ids.insert(session_id).second);
	return session_id;
}

std::optional<ConnectionSession> host_handshake(
	TcpConnection &connection,
	gamestate::player_id_t assigned_player,
	const AssetDigest &required_asset_digest) {
	if (assigned_player == 0) {
		throw Error{MSG(err) << "Lobby-assigned FVD player ID must be nonzero"};
	}

	auto frame = connection.receive_frame();
	if (not frame.has_value()) {
		throw Error{MSG(err) << "FVD client disconnected during handshake"};
	}
	require_handshake_envelope(*frame);
	if (frame->message_type != MessageType::CLIENT_HELLO) {
		throw Error{MSG(err) << "Expected FVD client hello, received message "
		                     << static_cast<uint16_t>(frame->message_type)};
	}
	if (frame->protocol_major != PROTOCOL_MAJOR) {
		send_rejection(connection, "protocol major version does not match host");
		return std::nullopt;
	}

	auto hello = decode_client_hello(frame->payload);
	auto rejection = compatibility_rejection(*frame, hello, required_asset_digest);
	if (not rejection.empty()) {
		send_rejection(connection, std::move(rejection));
		return std::nullopt;
	}

	SessionIdentity identity{generate_session_id(), assigned_player};
	ProtocolVersion negotiated_version{
		PROTOCOL_MAJOR,
		std::min(frame->protocol_minor, PROTOCOL_MINOR)};
	connection.send_frame(make_handshake_frame(
		MessageType::SERVER_ACCEPT,
		encode_server_accept({identity.session_id, identity.player_id}),
		negotiated_version));
	return ConnectionSession{
		identity,
		negotiated_version};
}

ConnectionSession client_handshake(TcpConnection &connection,
                                   const ClientHello &hello) {
	connection.send_frame(make_client_hello_frame(hello));
	auto response = connection.receive_frame();
	if (not response.has_value()) {
		throw Error{MSG(err) << "FVD host disconnected during handshake"};
	}
	require_server_reply(*response);
	if (response->message_type == MessageType::SERVER_REJECT) {
		auto rejection = decode_server_reject(response->payload);
		throw Error{MSG(err) << "FVD host rejected connection: " << rejection.reason};
	}
	if (response->protocol_major != PROTOCOL_MAJOR
	    or response->protocol_minor < hello.minimum_protocol_minor
	    or response->protocol_minor > PROTOCOL_MINOR) {
		throw Error{MSG(err) << "FVD host accepted with incompatible protocol version "
		                     << response->protocol_major << "."
		                     << response->protocol_minor};
	}

	auto accept = decode_server_accept(response->payload);
	return ConnectionSession{
		{accept.session_id, accept.player_id},
		{response->protocol_major, response->protocol_minor}};
}

} // namespace openage::network
