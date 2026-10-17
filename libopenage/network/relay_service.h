// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
// pxd: from libc.stdint cimport uint16_t, uint64_t
#include <cstdint>
// pxd: from libcpp.string cimport string
#include <string>
// pxd: from libcpp.vector cimport vector
#include <vector>

#include "../util/compiler.h"

namespace openage::network {

class RelayPairRegistry;
class TcpListener;

/**
 * Relay credentials passed through the Python command boundary.
 *
 * Tokens contain exactly 32 raw bytes.
 *
 * pxd:
 *
 * cppclass RelayServiceCredentials:
 *     uint64_t match_id
 *     uint64_t player_id
 *     string host_token
 *     string guest_token
 */
struct RelayServiceCredentials {
	uint64_t match_id{};
	uint64_t player_id{};
	std::string host_token{};
	std::string guest_token{};
};

/**
 * Admit and forward every pair already provisioned in a registry.
 *
 * The call returns after all provisioned pairs have connected and then
 * disconnected. New provisioning while this function runs is unsupported.
 */
void serve_relay_pairs(TcpListener &listener,
                       RelayPairRegistry &registry,
                       size_t pair_count);

/**
 * Run a one-match relay service from the command-line configuration.
 *
 * pxd: void run_relay_service(const string &address, uint16_t port, const vector[RelayServiceCredentials] &credentials) except +
 */
OAAPI void run_relay_service(
	const std::string &address,
	uint16_t port,
	const std::vector<RelayServiceCredentials> &credentials);

} // namespace openage::network
