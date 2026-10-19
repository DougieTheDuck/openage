// Copyright 2026 the openage authors. See copying.md for legal info.

#include "transport.h"

#include <array>
#include <cerrno>
#include <chrono>
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
#include <fcntl.h>
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
constexpr long SOCKET_WAIT_MICROSECONDS = 250000;

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

bool would_block_socket_call() {
#if defined(_WIN32)
	return WSAGetLastError() == WSAEWOULDBLOCK;
#else
	return errno == EAGAIN or errno == EWOULDBLOCK;
#endif
}

void make_socket_nonblocking(native_socket_t socket) {
#if defined(_WIN32)
	u_long enabled = 1;
	if (ioctlsocket(socket, FIONBIO, &enabled) != 0) {
		throw Error{MSG(err) << "Could not make FVD TCP socket nonblocking: "
		                     << last_socket_error()};
	}
#else
	auto flags = fcntl(socket, F_GETFL, 0);
	if (flags < 0 or fcntl(socket, F_SETFL, flags | O_NONBLOCK) != 0) {
		throw Error{MSG(err) << "Could not make FVD TCP socket nonblocking: "
		                     << last_socket_error()};
	}
#endif
}

int wait_for_socket(native_socket_t socket, bool writable) {
	fd_set sockets{};
	FD_ZERO(&sockets);
	FD_SET(socket, &sockets);
	timeval timeout{};
	timeout.tv_usec = SOCKET_WAIT_MICROSECONDS;
#if defined(_WIN32)
	constexpr int descriptor_count = 0;
#else
	auto descriptor_count = socket + 1;
#endif
	return select(descriptor_count,
	              writable ? nullptr : &sockets,
	              writable ? &sockets : nullptr,
	              nullptr,
	              &timeout);
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
	try {
		make_socket_nonblocking(connected);
	}
	catch (...) {
		close_socket(connected);
		throw;
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
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
	while (offset < bytes.size()) {
		// A silent or backpressured peer must not block the relay worker
		// forever, preventing heartbeat processing or a clean match reset.
		if (std::chrono::steady_clock::now() >= deadline) {
			throw Error{MSG(err) << "Timed out sending an FVD TCP frame (15s)"};
		}
		if (this->socket_handle.load() != socket_handle) {
			throw Error{MSG(err) << "FVD TCP connection closed during frame send"};
		}
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
		if (sent < 0 && would_block_socket_call()) {
			auto ready = wait_for_socket(to_native(socket_handle), true);
			if (ready >= 0) {
				continue;
			}
			if (interrupted_socket_call()) {
				continue;
			}
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
		if (this->socket_handle.load() != socket_handle) {
			socket_lock.unlock();
			return std::nullopt;
		}
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
		if (would_block_socket_call()) {
			auto ready = wait_for_socket(to_native(socket_handle), false);
			if (ready >= 0) {
				continue;
			}
			if (interrupted_socket_call()) {
				continue;
			}
		}
		if (this->socket_handle.load() != socket_handle) {
			socket_lock.unlock();
			return std::nullopt;
		}
		auto error = last_socket_error();
		socket_lock.unlock();
		this->close();
		throw Error{MSG(err) << "Could not receive FVD TCP frame: " << error};
	}
}

std::optional<Frame> TcpConnection::try_receive_frame() {
	if (not this->pending_frames.empty()) {
		auto frame = std::move(this->pending_frames.front());
		this->pending_frames.pop_front();
		return frame;
	}
	std::shared_lock socket_lock{this->socket_lifetime_mutex};
	auto handle = this->socket_handle.load();
	if (handle == INVALID_SOCKET_HANDLE) {
		return std::nullopt;
	}
	std::array<uint8_t, 4096> buffer{};
	while (this->socket_handle.load() == handle) {
		auto count = ::recv(to_native(handle), reinterpret_cast<char *>(buffer.data()),
		                    static_cast<int>(buffer.size()), 0);
		if (count > 0) {
			for (auto &frame : this->parser.push(buffer.data(), static_cast<size_t>(count))) {
				this->pending_frames.push_back(std::move(frame));
			}
			if (not this->pending_frames.empty()) {
				auto frame = std::move(this->pending_frames.front());
				this->pending_frames.pop_front();
				return frame;
			}
			continue; // A partial frame may have another chunk ready.
		}
		if (count == 0) {
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
		if (would_block_socket_call()) {
			return std::nullopt;
		}
		if (this->socket_handle.load() != handle) {
			return std::nullopt;
		}
		auto error = last_socket_error();
		socket_lock.unlock();
		this->close();
		throw Error{MSG(err) << "Could not receive FVD TCP frame: " << error};
	}
	return std::nullopt;
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

bool TcpConnection::peer_connected() const {
	std::shared_lock socket_lock{this->socket_lifetime_mutex};
	auto handle = this->socket_handle.load();
	if (handle == INVALID_SOCKET_HANDLE) {
		return false;
	}

	// The FVD TCP sockets are nonblocking. MSG_PEEK lets the relay detect
	// a waiting host's FIN without stealing any pending protocol data.
	char byte{};
	const auto received = ::recv(to_native(handle), &byte, 1, MSG_PEEK);
	if (received > 0) {
		return true;
	}
	if (received == 0) {
		return false; // Peer completed an orderly shutdown.
	}
	return interrupted_socket_call() or would_block_socket_call();
}

TcpListener::TcpListener() :
	socket_handle{INVALID_SOCKET_HANDLE},
	port{0},
	socket_lifetime_mutex{} {}

TcpListener::TcpListener(uintptr_t socket_handle, uint16_t port) :
	socket_handle{socket_handle},
	port{port},
	socket_lifetime_mutex{} {}

TcpListener::~TcpListener() {
	this->close();
}

TcpListener::TcpListener(TcpListener &&other) noexcept :
	socket_handle{other.socket_handle.exchange(INVALID_SOCKET_HANDLE)},
	port{other.port.exchange(0)},
	socket_lifetime_mutex{} {}

TcpListener &TcpListener::operator=(TcpListener &&other) noexcept {
	if (this != &other) {
		this->close();
		this->socket_handle.store(other.socket_handle.exchange(INVALID_SOCKET_HANDLE));
		this->port.store(other.port.exchange(0));
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
	try {
		make_socket_nonblocking(listening);
	}
	catch (...) {
		close_socket(listening);
		throw;
	}
	return TcpListener{from_native(listening), socket_port(listening)};
}

TcpConnection TcpListener::accept() {
	std::shared_lock socket_lock{this->socket_lifetime_mutex};
	auto socket_handle = this->socket_handle.load();
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		throw Error{MSG(err) << "Cannot accept from a closed TCP listener"};
	}

	native_socket_t accepted = INVALID_NATIVE_SOCKET;
	while (accepted == INVALID_NATIVE_SOCKET) {
		if (this->socket_handle.load() != socket_handle) {
			throw Error{MSG(err) << "FVD TCP listener closed during accept"};
		}
		accepted = ::accept(to_native(socket_handle), nullptr, nullptr);
		if (accepted != INVALID_NATIVE_SOCKET) {
			break;
		}
		if (interrupted_socket_call()) {
			continue;
		}
		if (would_block_socket_call()) {
			auto ready = wait_for_socket(to_native(socket_handle), false);
			if (ready >= 0 or interrupted_socket_call()) {
				continue;
			}
		}
		if (this->socket_handle.load() != socket_handle) {
			throw Error{MSG(err) << "FVD TCP listener closed during accept"};
		}
		throw Error{MSG(err) << "Could not accept FVD TCP connection: "
		                     << last_socket_error()};
	}
	prevent_sigpipe(accepted);
	try {
		make_socket_nonblocking(accepted);
	}
	catch (...) {
		close_socket(accepted);
		throw;
	}
	return TcpConnection{from_native(accepted)};
}

uint16_t TcpListener::get_port() const {
	return this->port.load();
}

void TcpListener::close() {
	auto socket_handle = this->socket_handle.exchange(INVALID_SOCKET_HANDLE);
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		return;
	}
	this->port.store(0);
	auto socket = to_native(socket_handle);
	shutdown_socket(socket);
	std::unique_lock socket_lock{this->socket_lifetime_mutex};
	close_socket(socket);
}

bool TcpListener::is_open() const {
	return this->socket_handle.load() != INVALID_SOCKET_HANDLE;
}

} // namespace openage::network
