# Copyright 2026 the openage authors. See copying.md for legal info.

from libc.stdint cimport uint16_t
from libcpp.string cimport string
from libcpp.vector cimport vector

from libopenage.error.handlers cimport set_exit_ok
from libopenage.network.relay_service cimport (
    RelayServiceCredentials,
    run_relay_service,
)


def run_relay(str address, int port, pairs):
    """Translate command-line relay configuration and run the C++ service."""
    cdef string address_cpp = address.encode("utf-8")
    cdef uint16_t port_cpp = port
    cdef vector[RelayServiceCredentials] credentials
    cdef RelayServiceCredentials pair

    for match_id, player_id, host_token, guest_token in pairs:
        pair.match_id = match_id
        pair.player_id = player_id
        pair.host_token = host_token
        pair.guest_token = guest_token
        credentials.push_back(pair)

    set_exit_ok(False)
    try:
        with nogil:
            run_relay_service(address_cpp, port_cpp, credentials)
        return 0
    finally:
        set_exit_ok(True)
