// Copyright 2026 the openage authors. See copying.md for legal info.

#include "transport.h"

#include <array>
#include <cerrno>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include "error/error.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace openage::network {

namespace {

#if defined(_WIN32)
using native_socket_t = SOCKET;
constexpr native_socket_t INVALID_NATIVE_SOCKET = INVALID_SOCKET;
#else
using native_socket_t = int;
constexpr native_socket_t INVALID_NATIVE_SOCKET = -1;
#endif

constexpr uintptr_t INVALID_SOCKET_HANDLE = (std::numeric_limits<uintptr_t>::max)();
constexpr int LISTEN_BACKLOG = 8;

native_socket_t to_native(uintptr_t handle) {
	return static_cast<native_socket_t>(handle);
}

uintptr_t from_native(native_socket_t socket) {
	return static_cast<uintptr_t>(socket);
}

#if defined(_WIN32)
class SocketRuntime {
public:
	SocketRuntime() {
		WSADATA data{};
		auto result = WSAStartup(MAKEWORD(2, 2), &data);
		if (result != 0) {
			throw Error{MSG(err) << "Could not initialize Winsock: " << result};
		}
	}

	~SocketRuntime() {
		WSACleanup();
	}
};
#endif

void ensure_socket_runtime() {
#if defined(_WIN32)
	static SocketRuntime runtime{};
	(void) runtime;
#endif
}

int last_socket_error() {
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif
}

bool interrupted_socket_call() {
#if defined(_WIN32)
	return WSAGetLastError() == WSAEINTR;
#else
	return errno == EINTR;
#endif
}

void close_socket(native_socket_t socket) {
	if (socket == INVALID_NATIVE_SOCKET) {
		return;
	}
#if defined(_WIN32)
	closesocket(socket);
#else
	::close(socket);
#endif
}

void shutdown_socket(native_socket_t socket) {
#if defined(_WIN32)
	::shutdown(socket, SD_BOTH);
#else
	::shutdown(socket, SHUT_RDWR);
#endif
}

void prevent_sigpipe(native_socket_t socket) {
#if defined(SO_NOSIGPIPE)
	int enabled = 1;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE,
	           reinterpret_cast<const char *>(&enabled), sizeof(enabled));
#else
	(void) socket;
#endif
}

int send_flags() {
#if defined(MSG_NOSIGNAL)
	return MSG_NOSIGNAL;
#else
	return 0;
#endif
}

std::string service_for(uint16_t port) {
	return std::to_string(port);
}

uint16_t socket_port(native_socket_t socket) {
	sockaddr_storage address{};
#if defined(_WIN32)
	int address_size = sizeof(address);
#else
	socklen_t address_size = sizeof(address);
#endif
	if (getsockname(socket, reinterpret_cast<sockaddr *>(&address), &address_size) != 0) {
		throw Error{MSG(err) << "Could not query TCP listener port: " << last_socket_error()};
	}

	if (address.ss_family == AF_INET) {
		auto ipv4 = reinterpret_cast<const sockaddr_in *>(&address);
		return ntohs(ipv4->sin_port);
	}
	if (address.ss_family == AF_INET6) {
		auto ipv6 = reinterpret_cast<const sockaddr_in6 *>(&address);
		return ntohs(ipv6->sin6_port);
	}
	throw Error{MSG(err) << "TCP listener has an unsupported address family"};
}

} // namespace

TcpConnection::TcpConnection() :
	socket_handle{INVALID_SOCKET_HANDLE},
	socket_lifetime_mutex{},
	parser{},
	pending_frames{} {}

TcpConnection::TcpConnection(uintptr_t socket_handle) :
	socket_handle{socket_handle},
	socket_lifetime_mutex{},
	parser{},
	pending_frames{} {}

TcpConnection::~TcpConnection() {
	this->close();
}

TcpConnection::TcpConnection(TcpConnection &&other) noexcept :
	socket_handle{other.socket_handle.exchange(INVALID_SOCKET_HANDLE)},
	socket_lifetime_mutex{},
	parser{std::move(other.parser)},
	pending_frames{std::move(other.pending_frames)} {}

TcpConnection &TcpConnection::operator=(TcpConnection &&other) noexcept {
	if (this != &other) {
		this->close();
		this->socket_handle.store(other.socket_handle.exchange(INVALID_SOCKET_HANDLE));
		this->parser = std::move(other.parser);
		this->pending_frames = std::move(other.pending_frames);
	}
	return *this;
}

TcpConnection TcpConnection::connect(const std::string &address, uint16_t port) {
	ensure_socket_runtime();

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	addrinfo *addresses = nullptr;
	auto service = service_for(port);
	auto resolution = getaddrinfo(address.c_str(), service.c_str(), &hints, &addresses);
	if (resolution != 0) {
		throw Error{MSG(err) << "Could not resolve TCP address " << address
		                     << ":" << port << ": " << resolution};
	}

	native_socket_t connected = INVALID_NATIVE_SOCKET;
	for (auto candidate = addresses; candidate != nullptr; candidate = candidate->ai_next) {
		auto socket = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
		if (socket == INVALID_NATIVE_SOCKET) {
			continue;
		}
		prevent_sigpipe(socket);
		if (::connect(socket, candidate->ai_addr, static_cast<int>(candidate->ai_addrlen)) == 0) {
			connected = socket;
			break;
		}
		close_socket(socket);
	}
	freeaddrinfo(addresses);

	if (connected == INVALID_NATIVE_SOCKET) {
		throw Error{MSG(err) << "Could not connect to TCP endpoint " << address
		                     << ":" << port << ": " << last_socket_error()};
	}
	return TcpConnection{from_native(connected)};
}

void TcpConnection::send_frame(const Frame &frame) {
	std::shared_lock socket_lock{this->socket_lifetime_mutex};
	auto socket_handle = this->socket_handle.load();
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		throw Error{MSG(err) << "Cannot send an FVD frame on a closed TCP connection"};
	}

	auto bytes = encode_frame(frame);
	size_t offset = 0;
	while (offset < bytes.size()) {
		auto remaining = bytes.size() - offset;
		auto chunk_size = remaining > static_cast<size_t>((std::numeric_limits<int>::max)())
		                    ? (std::numeric_limits<int>::max)()
		                    : static_cast<int>(remaining);
		auto sent = ::send(to_native(socket_handle),
		                   reinterpret_cast<const char *>(bytes.data() + offset),
		                   chunk_size,
		                   send_flags());
		if (sent > 0) {
			offset += static_cast<size_t>(sent);
			continue;
		}
		if (sent < 0 && interrupted_socket_call()) {
			continue;
		}
		throw Error{MSG(err) << "Could not send FVD TCP frame: " << last_socket_error()};
	}
}

std::optional<Frame> TcpConnection::receive_frame() {
	if (not this->pending_frames.empty()) {
		auto frame = std::move(this->pending_frames.front());
		this->pending_frames.pop_front();
		return frame;
	}
	std::shared_lock socket_lock{this->socket_lifetime_mutex};
	auto socket_handle = this->socket_handle.load();
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		return std::nullopt;
	}

	std::array<uint8_t, 4096> buffer{};
	while (true) {
		auto received = ::recv(to_native(socket_handle),
		                       reinterpret_cast<char *>(buffer.data()),
		                       static_cast<int>(buffer.size()),
		                       0);
		if (received > 0) {
			auto frames = this->parser.push(buffer.data(), static_cast<size_t>(received));
			for (auto &frame : frames) {
				this->pending_frames.push_back(std::move(frame));
			}
			if (not this->pending_frames.empty()) {
				auto frame = std::move(this->pending_frames.front());
				this->pending_frames.pop_front();
				return frame;
			}
			continue;
		}
		if (received == 0) {
			socket_lock.unlock();
			this->close();
			if (this->parser.buffered_size() > 0) {
				throw Error{MSG(err) << "TCP peer closed with an incomplete FVD frame"};
			}
			return std::nullopt;
		}
		if (interrupted_socket_call()) {
			continue;
		}
		auto error = last_socket_error();
		socket_lock.unlock();
		this->close();
		throw Error{MSG(err) << "Could not receive FVD TCP frame: " << error};
	}
}

void TcpConnection::close() {
	auto socket_handle = this->socket_handle.exchange(INVALID_SOCKET_HANDLE);
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		return;
	}
	auto socket = to_native(socket_handle);
	shutdown_socket(socket);
	std::unique_lock socket_lock{this->socket_lifetime_mutex};
	close_socket(socket);
}

bool TcpConnection::is_open() const {
	return this->socket_handle.load() != INVALID_SOCKET_HANDLE;
}

TcpListener::TcpListener() :
	socket_handle{INVALID_SOCKET_HANDLE},
	port{0} {}

TcpListener::TcpListener(uintptr_t socket_handle, uint16_t port) :
	socket_handle{socket_handle},
	port{port} {}

TcpListener::~TcpListener() {
	this->close();
}

TcpListener::TcpListener(TcpListener &&other) noexcept :
	socket_handle{std::exchange(other.socket_handle, INVALID_SOCKET_HANDLE)},
	port{std::exchange(other.port, 0)} {}

TcpListener &TcpListener::operator=(TcpListener &&other) noexcept {
	if (this != &other) {
		this->close();
		this->socket_handle = std::exchange(other.socket_handle, INVALID_SOCKET_HANDLE);
		this->port = std::exchange(other.port, 0);
	}
	return *this;
}

TcpListener TcpListener::listen(const std::string &address, uint16_t port) {
	ensure_socket_runtime();

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	addrinfo *addresses = nullptr;
	auto service = service_for(port);
	const char *node = address.empty() ? nullptr : address.c_str();
	auto resolution = getaddrinfo(node, service.c_str(), &hints, &addresses);
	if (resolution != 0) {
		throw Error{MSG(err) << "Could not resolve TCP listen address " << address
		                     << ":" << port << ": " << resolution};
	}

	native_socket_t listening = INVALID_NATIVE_SOCKET;
	for (auto candidate = addresses; candidate != nullptr; candidate = candidate->ai_next) {
		auto socket = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
		if (socket == INVALID_NATIVE_SOCKET) {
			continue;
		}
		int reuse_address = 1;
		setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
		           reinterpret_cast<const char *>(&reuse_address), sizeof(reuse_address));
		prevent_sigpipe(socket);
		if (::bind(socket, candidate->ai_addr, static_cast<int>(candidate->ai_addrlen)) == 0
		    && ::listen(socket, LISTEN_BACKLOG) == 0) {
			listening = socket;
			break;
		}
		close_socket(socket);
	}
	freeaddrinfo(addresses);

	if (listening == INVALID_NATIVE_SOCKET) {
		throw Error{MSG(err) << "Could not listen on TCP endpoint " << address
		                     << ":" << port << ": " << last_socket_error()};
	}
	return TcpListener{from_native(listening), socket_port(listening)};
}

TcpConnection TcpListener::accept() {
	if (not this->is_open()) {
		throw Error{MSG(err) << "Cannot accept from a closed TCP listener"};
	}

	native_socket_t accepted = INVALID_NATIVE_SOCKET;
	while (accepted == INVALID_NATIVE_SOCKET) {
		accepted = ::accept(to_native(this->socket_handle), nullptr, nullptr);
		if (accepted == INVALID_NATIVE_SOCKET && not interrupted_socket_call()) {
			throw Error{MSG(err) << "Could not accept FVD TCP connection: "
			                     << last_socket_error()};
		}
	}
	prevent_sigpipe(accepted);
	return TcpConnection{from_native(accepted)};
}

uint16_t TcpListener::get_port() const {
	return this->port;
}

void TcpListener::close() {
	if (not this->is_open()) {
		return;
	}
	auto socket = to_native(this->socket_handle);
	this->socket_handle = INVALID_SOCKET_HANDLE;
	this->port = 0;
	shutdown_socket(socket);
	close_socket(socket);
}

bool TcpListener::is_open() const {
	return this->socket_handle != INVALID_SOCKET_HANDLE;
}

} // namespace openage::network
