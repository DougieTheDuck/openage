// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <optional>
#include <shared_mutex>
#include <string>

#include "network/protocol.h"

namespace openage::network {

/**
 * One blocking TCP connection carrying FVD protocol frames.
 *
 * The connection owns its socket and is movable but not copyable. Methods
 * throw Error for socket failures and malformed frames.
 */
class TcpConnection {
public:
	TcpConnection();
	~TcpConnection();

	TcpConnection(const TcpConnection &) = delete;
	TcpConnection &operator=(const TcpConnection &) = delete;
	TcpConnection(TcpConnection &&other) noexcept;
	TcpConnection &operator=(TcpConnection &&other) noexcept;

	/**
	 * Connect to a listening TCP endpoint.
	 *
	 * @param address IPv4, IPv6 or DNS address.
	 * @param port TCP port in host byte order.
	 */
	static TcpConnection connect(const std::string &address, uint16_t port);

	/**
	 * Send one complete protocol frame, blocking until every byte is sent.
	 *
	 * One thread may send while another receives on the same connection.
	 */
	void send_frame(const Frame &frame);

	/**
	 * Receive one complete protocol frame.
	 *
	 * @return The next frame, or nullopt after an orderly peer shutdown.
	 * One thread may receive while another sends on the same connection.
	 */
	std::optional<Frame> receive_frame();

	/** Close the connection. Calling this more than once has no effect. */
	void close();

	/** Whether this object currently owns an open socket. */
	bool is_open() const;

private:
	explicit TcpConnection(uintptr_t socket_handle);

	std::atomic<uintptr_t> socket_handle;
	mutable std::shared_mutex socket_lifetime_mutex;
	FrameParser parser;
	std::deque<Frame> pending_frames;

	friend class TcpListener;
};

/** A blocking TCP listener for direct-IP FVD sessions. */
class TcpListener {
public:
	TcpListener();
	~TcpListener();

	TcpListener(const TcpListener &) = delete;
	TcpListener &operator=(const TcpListener &) = delete;
	TcpListener(TcpListener &&other) noexcept;
	TcpListener &operator=(TcpListener &&other) noexcept;

	/**
	 * Bind and listen on an address.
	 *
	 * An empty address listens on all local interfaces. Port zero requests an
	 * ephemeral port, which can be queried with get_port().
	 */
	static TcpListener listen(const std::string &address, uint16_t port);

	/** Block until one client connects. */
	TcpConnection accept();

	/** Get the bound TCP port in host byte order. */
	uint16_t get_port() const;

	/** Close the listener. Calling this more than once has no effect. */
	void close();

	/** Whether this object currently owns an open socket. */
	bool is_open() const;

private:
	TcpListener(uintptr_t socket_handle, uint16_t port);

	uintptr_t socket_handle;
	uint16_t port;
};

} // namespace openage::network
