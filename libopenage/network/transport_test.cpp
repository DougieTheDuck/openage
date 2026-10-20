// Copyright 2026 the openage authors. See copying.md for legal info.

#include "transport.h"

#include <chrono>
#include <exception>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "testing/testing.h"

namespace openage::network::tests {

void transport() {
	auto listener = TcpListener::listen("127.0.0.1", 0);
	TESTEQUALS(listener.is_open(), true);
	listener.get_port() != 0 or TESTFAIL;

	std::exception_ptr server_error{};
	std::jthread server{[&]() {
		try {
			auto connection = listener.accept();
			auto first = connection.receive_frame();
			first.has_value() or TESTFAIL;
			first->message_type == MessageType::COMMAND_INTENT or TESTFAIL;
			TESTEQUALS(first->session_id, uint64_t{42});
			TESTEQUALS(first->sequence, uint64_t{1});
			first->payload == std::vector<uint8_t>({0x01, 0x02}) or TESTFAIL;

			auto second = connection.receive_frame();
			second.has_value() or TESTFAIL;
			second->message_type == MessageType::PING or TESTFAIL;
			TESTEQUALS(second->sequence, uint64_t{2});

			Frame response{};
			response.message_type = MessageType::WORLD_DELTA;
			response.session_id = 42;
			response.sequence = 1;
			response.payload = {0x10, 0x20, 0x30};
			connection.send_frame(response);
		}
		catch (...) {
			server_error = std::current_exception();
		}
	}};

	auto client = TcpConnection::connect("127.0.0.1", listener.get_port());
	TESTEQUALS(client.is_open(), true);
	// Polling must not block or confuse "no frame yet" with disconnect.
	TESTEQUALS(client.try_receive_frame().has_value(), false);
	TESTEQUALS(client.is_open(), true);
	TESTEQUALS(client.peer_connected(), true);

	Frame command{};
	command.message_type = MessageType::COMMAND_INTENT;
	command.session_id = 42;
	command.sequence = 1;
	command.payload = {0x01, 0x02};
	client.send_frame(command);

	Frame ping{};
	ping.message_type = MessageType::PING;
	ping.session_id = 42;
	ping.sequence = 2;
	client.send_frame(ping);

	// Checking liveness must not eat incoming gameplay frames.
	TESTEQUALS(client.peer_connected(), true);
	auto response = client.receive_frame();
	response.has_value() or TESTFAIL;
	response->message_type == MessageType::WORLD_DELTA or TESTFAIL;
	TESTEQUALS(response->session_id, uint64_t{42});
	TESTEQUALS(response->sequence, uint64_t{1});
	response->payload == std::vector<uint8_t>({0x10, 0x20, 0x30}) or TESTFAIL;
	TESTEQUALS(client.receive_frame().has_value(), false);
	TESTEQUALS(client.peer_connected(), false);

	server.join();
	if (server_error) {
		std::rethrow_exception(server_error);
	}

	// Test the open-socket / disconnected-peer distinction independently:
	// the relay holds open sockets after an abandoned room's host leaves.
	auto abandoned_host = TcpConnection::connect("127.0.0.1", listener.get_port());
	auto relay_socket = listener.accept();
	TESTEQUALS(relay_socket.peer_connected(), true);
	abandoned_host.close();
	bool detected = false;
	for (size_t attempt = 0; attempt < 50; ++attempt) {
		if (not relay_socket.peer_connected()) {
			detected = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}
	TESTEQUALS(detected, true);
	TESTEQUALS(relay_socket.is_open(), true);
}

// The POSIX select/FD_SET implementation used to corrupt memory when
// the descriptor index reached FD_SETSIZE (normally 1024). Exercise an
// actual TCP read above that boundary, not merely poll() on a regular file.
void transport_high_fd() {
#if !defined(_WIN32)
	rlimit limits{};
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0
	    or limits.rlim_cur <= static_cast<rlim_t>(FD_SETSIZE + 64)) {
		// Some CI sandboxes cap the process at 1024 open descriptors.
		// Such environments cannot reproduce the high-FD scenario.
		return;
	}
	struct HeldDescriptors {
		std::vector<int> handles{};
		~HeldDescriptors() {
			for (auto handle : handles) {
				::close(handle);
			}
		}
	} occupied{};
	int highest = -1;
	while (highest <= FD_SETSIZE + 8) {
		highest = ::open("/dev/null", O_RDONLY);
		if (highest < 0) {
			// Constrained environment: did not reach the historical
			// FD_SET limit; avoid failing unrelated networking checks.
			return;
		}
		occupied.handles.push_back(highest);
	}
	auto listener = TcpListener::listen("127.0.0.1", 0);
	std::exception_ptr server_error{};
	std::jthread server{[&]() {
		try {
			auto connected = listener.accept();
			// Ensure the client actually waits for readability on its
			// high-valued socket rather than receiving prebuffered data.
			std::this_thread::sleep_for(std::chrono::milliseconds{50});
			Frame message{};
			message.message_type = MessageType::PING;
			message.session_id = 73;
			message.sequence = 1;
			connected.send_frame(message);
		}
		catch (...) {
			server_error = std::current_exception();
		}
	}};
	auto client = TcpConnection::connect("127.0.0.1", listener.get_port());
	auto received = client.receive_frame();
	TESTEQUALS(received.has_value(), true);
	TESTEQUALS(received->message_type, MessageType::PING);
	TESTEQUALS(received->session_id, uint64_t{73});
	client.close();
	listener.close();
	server.join();
	if (server_error) std::rethrow_exception(server_error);
#endif
}

} // namespace openage::network::tests
