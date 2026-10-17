# Copyright 2026 the openage authors. See copying.md for legal info.

add_sources(libopenage
	bounded_mailbox.cpp
	command_bridge.cpp
	command_mailbox.cpp
	command_mailbox_test.cpp
	connection_mailbox.cpp
	connection_mailbox_test.cpp
	fvd_loopback_test.cpp
	handshake.cpp
	handshake_test.cpp
	match_connection_state.cpp
	match_disconnect_coordinator.cpp
	match_disconnect_coordinator_test.cpp
	messages.cpp
	messages_test.cpp
	protocol.cpp
	protocol_test.cpp
	replication.cpp
	replication_test.cpp
	serialization.cpp
	session.cpp
	session_test.cpp
	transport.cpp
	transport_test.cpp
)
