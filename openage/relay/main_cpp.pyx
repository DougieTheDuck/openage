# Copyright 2026 the openage authors. See copying.md for legal info.

from libc.stddef cimport size_t
from libc.stdint cimport uint16_t
from libcpp.string cimport string
from libcpp.vector cimport vector

from libopenage.error.handlers cimport set_exit_ok
from libopenage.network.relay_service cimport run_relay_service, stop_relay_service


def run_relay(str address, int port, tokens, int max_rooms, int max_connections):
    """Translate command-line relay configuration and run the C++ service."""
    cdef string address_cpp = address.encode("utf-8")
    cdef uint16_t port_cpp = port
    cdef vector[string] authorized_tokens
    cdef size_t max_rooms_cpp = max_rooms
    cdef size_t max_connections_cpp = max_connections

    for token in tokens:
        authorized_tokens.push_back(token)

    set_exit_ok(False)
    try:
        with nogil:
            run_relay_service(
                address_cpp,
                port_cpp,
                authorized_tokens,
                max_rooms_cpp,
                max_connections_cpp,
            )
        return 0
    finally:
        set_exit_ok(True)


def stop_relay():
    """Interrupt the listener owned by the command-line relay."""
    with nogil:
        stop_relay_service()
