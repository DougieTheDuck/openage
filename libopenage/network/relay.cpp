// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "error/error.h"
#include "network/serialization.h"

namespace openage::network {

namespace {

constexpr size_t RELAY_CONNECT_PAYLOAD_SIZE = 52;
constexpr size_t RELAY_READY_PAYLOAD_SIZE = 16;
constexpr uint32_t MAX_RELAY_REJECTION_SIZE = 4096;

bool is_known_relay_role(RelayRole role) {
	return role == RelayRole::HOST or role == RelayRole::GUEST;
}

bool is_zero_token(const RelayToken &token) {
	return std::all_of(token.begin(), token.end(), [](uint8_t value) {
		return value == 0;
	});
}

bool tokens_equal(const RelayToken &left, const RelayToken &right) {
	uint8_t difference{};
	for (size_t i = 0; i < RELAY_TOKEN_SIZE; ++i) {
		difference |= left[i] ^ right[i];
	}
	return difference == 0;
}

void require_relay_connect(const RelayConnect &connect) {
	if (connect.match_id == 0) {
		throw Error{MSG(err) << "FVD relay match ID must be nonzero"};
	}
	if (connect.player_id == 0) {
		throw Error{MSG(err) << "FVD relay player ID must be nonzero"};
	}
	if (not is_known_relay_role(connect.role)) {
		throw Error{MSG(err) << "Unknown FVD relay role "
		                     << static_cast<uint16_t>(connect.role)};
	}
	if (is_zero_token(connect.token)) {
		throw Error{MSG(err) << "FVD relay token must not be empty"};
	}
}

void require_pair_credentials(const RelayPairCredentials &credentials) {
	RelayConnect host{
		credentials.match_id,
		credentials.player_id,
		RelayRole::HOST,
		credentials.host_token};
	RelayConnect guest{
		credentials.match_id,
		credentials.player_id,
		RelayRole::GUEST,
		credentials.guest_token};
	require_relay_connect(host);
	require_relay_connect(guest);
	if (tokens_equal(credentials.host_token, credentials.guest_token)) {
		throw Error{MSG(err) << "FVD relay host and guest tokens must be distinct"};
	}
}

Frame relay_frame(MessageType type, std::vector<uint8_t> payload) {
	Frame frame{};
	frame.message_type = type;
	frame.session_id = 0;
	frame.sequence = 0;
	frame.payload = std::move(payload);
	return frame;
}

void require_relay_envelope(const Frame &frame) {
	if (frame.protocol_major != PROTOCOL_MAJOR
	    or frame.protocol_minor < RELAY_PROTOCOL_MINOR
	    or frame.protocol_minor > PROTOCOL_MINOR) {
		throw Error{MSG(err) << "Incompatible FVD relay protocol version "
		                     << frame.protocol_major << "." << frame.protocol_minor};
	}
	if (frame.session_id != 0 or frame.sequence != 0) {
		throw Error{MSG(err) << "FVD relay admission requires zero session and sequence"};
	}
}

std::vector<uint8_t> encode_relay_ready(uint64_t match_id,
                                        gamestate::player_id_t player_id) {
	std::vector<uint8_t> payload{};
	payload.reserve(RELAY_READY_PAYLOAD_SIZE);
	append_u64(payload, match_id);
	append_u64(payload, player_id);
	return payload;
}

void require_relay_ready(const std::vector<uint8_t> &payload,
                         const RelayConnect &connect) {
	if (payload.size() != RELAY_READY_PAYLOAD_SIZE) {
		throw Error{MSG(err) << "Invalid FVD relay ready payload size " << payload.size()};
	}
	auto match_id = read_u64(payload.data());
	auto player_id = read_u64(payload.data() + sizeof(uint64_t));
	if (match_id != connect.match_id or player_id != connect.player_id) {
		throw Error{MSG(err) << "FVD relay ready response does not match requested pair"};
	}
}

std::vector<uint8_t> encode_relay_reject(const std::string &reason) {
	auto reason_size = std::min(reason.size(), static_cast<size_t>(MAX_RELAY_REJECTION_SIZE));
	std::vector<uint8_t> payload{};
	payload.reserve(sizeof(uint32_t) + reason_size);
	append_u32(payload, static_cast<uint32_t>(reason_size));
	payload.insert(payload.end(), reason.begin(), reason.begin() + reason_size);
	return payload;
}

std::string decode_relay_reject(const std::vector<uint8_t> &payload) {
	if (payload.size() < sizeof(uint32_t)) {
		throw Error{MSG(err) << "Truncated FVD relay rejection payload"};
	}
	auto reason_size = read_u32(payload.data());
	if (reason_size > MAX_RELAY_REJECTION_SIZE
	    or payload.size() != sizeof(uint32_t) + reason_size) {
		throw Error{MSG(err) << "Invalid FVD relay rejection payload size"};
	}
	return std::string{
		reinterpret_cast<const char *>(payload.data() + sizeof(uint32_t)),
		reason_size};
}

void reject_relay_connection(TcpConnection &connection, const std::string &reason) {
	try {
		connection.send_frame(relay_frame(
			MessageType::RELAY_REJECT,
			encode_relay_reject(reason)));
	}
	catch (...) {
		// A peer closing during rejection cannot affect the provisioned pair.
	}
	connection.close();
}

std::optional<RelayConnect> receive_relay_connect(TcpConnection &connection) {
	auto frame = connection.receive_frame();
	if (not frame.has_value()) {
		return std::nullopt;
	}
	require_relay_envelope(*frame);
	if (frame->message_type != MessageType::RELAY_CONNECT) {
		throw Error{MSG(err) << "Expected FVD relay admission, received message "
		                     << static_cast<uint16_t>(frame->message_type)};
	}
	return decode_relay_connect(frame->payload);
}

bool is_authorized(const RelayConnect &connect,
                   const RelayPairCredentials &credentials) {
	if (connect.match_id != credentials.match_id
	    or connect.player_id != credentials.player_id) {
		return false;
	}
	const auto &expected_token = connect.role == RelayRole::HOST ? credentials.host_token : credentials.guest_token;
	return tokens_equal(connect.token, expected_token);
}

} // namespace

std::vector<uint8_t> encode_relay_connect(const RelayConnect &connect) {
	require_relay_connect(connect);

	std::vector<uint8_t> payload{};
	payload.reserve(RELAY_CONNECT_PAYLOAD_SIZE);
	append_u64(payload, connect.match_id);
	append_u64(payload, connect.player_id);
	append_u16(payload, static_cast<uint16_t>(connect.role));
	append_u16(payload, 0);
	payload.insert(payload.end(), connect.token.begin(), connect.token.end());
	return payload;
}

RelayConnect decode_relay_connect(const std::vector<uint8_t> &payload) {
	if (payload.size() != RELAY_CONNECT_PAYLOAD_SIZE) {
		throw Error{MSG(err) << "Invalid FVD relay admission payload size " << payload.size()};
	}
	if (read_u16(payload.data() + 18) != 0) {
		throw Error{MSG(err) << "Unsupported FVD relay admission flags"};
	}

	RelayConnect connect{};
	connect.match_id = read_u64(payload.data());
	connect.player_id = read_u64(payload.data() + 8);
	connect.role = static_cast<RelayRole>(read_u16(payload.data() + 16));
	std::copy_n(payload.data() + 20, RELAY_TOKEN_SIZE, connect.token.begin());
	require_relay_connect(connect);
	return connect;
}

TcpConnection connect_relay(const std::string &address,
                            uint16_t port,
                            const RelayConnect &connect) {
	auto payload = encode_relay_connect(connect);
	auto connection = TcpConnection::connect(address, port);
	connection.send_frame(relay_frame(
		MessageType::RELAY_CONNECT,
		std::move(payload)));

	auto response = connection.receive_frame();
	if (not response.has_value()) {
		throw Error{MSG(err) << "FVD relay disconnected during admission"};
	}
	require_relay_envelope(*response);
	if (response->message_type == MessageType::RELAY_REJECT) {
		throw Error{MSG(err) << "FVD relay rejected connection: "
		                     << decode_relay_reject(response->payload)};
	}
	if (response->message_type != MessageType::RELAY_READY) {
		throw Error{MSG(err) << "Expected FVD relay admission reply, received message "
		                     << static_cast<uint16_t>(response->message_type)};
	}
	require_relay_ready(response->payload, connect);
	return connection;
}

RelayPair accept_relay_pair(TcpListener &listener,
                            const RelayPairCredentials &credentials) {
	require_pair_credentials(credentials);
	std::optional<TcpConnection> host{};
	std::optional<TcpConnection> guest{};

	while (not host.has_value() or not guest.has_value()) {
		auto connection = listener.accept();
		std::optional<RelayConnect> connect{};
		try {
			connect = receive_relay_connect(connection);
		}
		catch (...) {
			reject_relay_connection(connection, "malformed relay admission");
			continue;
		}
		if (not connect.has_value()) {
			continue;
		}
		if (not is_authorized(*connect, credentials)) {
			reject_relay_connection(connection, "relay credential was rejected");
			continue;
		}
		if (connect->role == RelayRole::HOST) {
			if (host.has_value()) {
				reject_relay_connection(connection, "relay host is already connected");
				continue;
			}
			host = std::move(connection);
		}
		else {
			if (guest.has_value()) {
				reject_relay_connection(connection, "relay guest is already connected");
				continue;
			}
			guest = std::move(connection);
		}
	}

	auto ready = relay_frame(
		MessageType::RELAY_READY,
		encode_relay_ready(credentials.match_id, credentials.player_id));
	host->send_frame(ready);
	guest->send_frame(ready);
	return {std::move(*host), std::move(*guest)};
}

void forward_relay_pair(RelayPair pair) {
	std::atomic_bool stopping{};
	std::mutex error_mutex{};
	std::exception_ptr relay_error{};

	auto stop = [&](std::exception_ptr error) {
		auto first_stop = not stopping.exchange(true);
		if (first_stop and error) {
			std::scoped_lock lock{error_mutex};
			relay_error = error;
		}
		pair.host.close();
		pair.guest.close();
	};
	auto forward = [&](TcpConnection &source, TcpConnection &destination) {
		try {
			while (auto frame = source.receive_frame()) {
				destination.send_frame(*frame);
			}
			stop(nullptr);
		}
		catch (...) {
			if (not stopping.load()) {
				stop(std::current_exception());
			}
		}
	};

	std::jthread host_to_guest{forward, std::ref(pair.host), std::ref(pair.guest)};
	forward(pair.guest, pair.host);
	host_to_guest.join();
	if (relay_error) {
		std::rethrow_exception(relay_error);
	}
}

} // namespace openage::network
