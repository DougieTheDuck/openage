// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

// pxd: from libc.stddef cimport size_t
#include <cstddef>
// pxd: from libc.stdint cimport uint16_t
#include <cstdint>
// pxd: from libcpp.string cimport string
#include <string>
// pxd: from libcpp.vector cimport vector
#include <vector>

#include "../util/compiler.h"

namespace openage::network {

class RelayRoomDirectory;
class TcpListener;

/**
 * Accept room directory requests and forward every completed room.
 *
 * The service runs until its listener is closed. At most max_connections
 * clients may be processing requests or forwarding a match concurrently.
 */
void serve_relay_directory(TcpListener &listener,
                           RelayRoomDirectory &directory,
                           size_t max_connections);

/**
 * Run a persistent public or token-restricted relay service.
 *
 * Each token contains exactly 32 raw bytes. An empty token list enables public
 * access.
 *
 * pxd: void run_relay_service(const string &address, uint16_t port, const vector[string] &authorized_tokens, size_t max_rooms, size_t max_connections) except +
 */
OAAPI void run_relay_service(
	const std::string &address,
	uint16_t port,
	const std::vector<std::string> &authorized_tokens,
	size_t max_rooms,
	size_t max_connections);

/**
 * Close the listener owned by a running command-line relay.
 *
 * pxd: void stop_relay_service() noexcept
 */
OAAPI void stop_relay_service() noexcept;

} // namespace openage::network
