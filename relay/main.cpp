// Copyright 2026 the openage authors. See copying.md for legal info.

/*
 * Standalone FVD room relay.
 *
 * The same relay as `openage relay`, built without Python, Qt, graphics or
 * audio, so it can run on a headless server. It accepts the same options.
 */

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <pthread.h>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "log/stdout_logsink.h"
#include "network/relay_service.h"


namespace {

constexpr uint16_t default_port = 32456;
constexpr size_t default_max_rooms = 64;
constexpr size_t default_max_connections = 128;

void print_usage(const char *program) {
	std::cout
		<< "usage: " << program << " [options]\n"
		<< "\n"
		<< "Run the FVD room relay (the server players connect to for multiplayer).\n"
		<< "\n"
		<< "options:\n"
		<< "  --listen ADDRESS        IPv4, IPv6 or DNS address to accept connections on\n"
		<< "                          (default: 0.0.0.0)\n"
		<< "  --port PORT             TCP port to accept connections on (default: 32456)\n"
		<< "  --player-token TOKEN    authorize one reusable 64-character hex player token;\n"
		<< "                          repeat for each alpha player (omit for public access)\n"
		<< "  --max-rooms N           maximum advertised rooms waiting for a guest\n"
		<< "                          (1-4096, default: 64)\n"
		<< "  --max-connections N     maximum concurrent relay requests and matches\n"
		<< "                          (default: 128)\n"
		<< "  -h, --help              show this help and exit\n"
		<< "\n"
		<< "Stop the relay with Ctrl+C or SIGTERM.\n";
}

[[noreturn]] void fail(const char *program, const std::string &message) {
	std::cerr << program << ": error: " << message << "\n"
	          << "Run '" << program << " --help' for the available options.\n";
	std::exit(2);
}

std::optional<unsigned long long> parse_number(std::string_view text) {
	if (text.empty()) {
		return std::nullopt;
	}
	unsigned long long value = 0;
	for (char c : text) {
		if (c < '0' or c > '9') {
			return std::nullopt;
		}
		value = value * 10 + static_cast<unsigned>(c - '0');
		if (value > 1'000'000'000ull) {
			return std::nullopt;
		}
	}
	return value;
}

/** Same rules as openage/relay/main.py: 32 bytes of hex, not all zero. */
std::optional<std::string> parse_token(std::string_view hex) {
	if (hex.size() != 64) {
		return std::nullopt;
	}
	auto nibble = [](char c) -> int {
		if (c >= '0' and c <= '9') return c - '0';
		if (c >= 'a' and c <= 'f') return c - 'a' + 10;
		if (c >= 'A' and c <= 'F') return c - 'A' + 10;
		return -1;
	};
	std::string token;
	token.reserve(32);
	bool any_nonzero = false;
	for (size_t i = 0; i < hex.size(); i += 2) {
		const int high = nibble(hex[i]);
		const int low = nibble(hex[i + 1]);
		if (high < 0 or low < 0) {
			return std::nullopt;
		}
		const auto byte = static_cast<char>((high << 4) | low);
		any_nonzero = any_nonzero or byte != 0;
		token.push_back(byte);
	}
	if (not any_nonzero) {
		return std::nullopt;
	}
	return token;
}

} // namespace


int main(int argc, char **argv) {
	const char *program = argc > 0 ? argv[0] : "openage-relay";

	std::string listen = "0.0.0.0";
	uint16_t port = default_port;
	std::vector<std::string> tokens;
	size_t max_rooms = default_max_rooms;
	size_t max_connections = default_max_connections;

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "-h" or arg == "--help") {
			print_usage(program);
			return 0;
		}

		// Accept both "--option value" and "--option=value".
		std::string_view name = arg;
		std::optional<std::string_view> value;
		if (const auto eq = arg.find('='); arg.starts_with("--") and eq != std::string_view::npos) {
			name = arg.substr(0, eq);
			value = arg.substr(eq + 1);
		}
		auto take_value = [&]() -> std::string_view {
			if (value) {
				return *value;
			}
			if (i + 1 >= argc) {
				fail(program, std::string{name} + " needs a value");
			}
			return argv[++i];
		};

		if (name == "--listen") {
			listen = std::string{take_value()};
		}
		else if (name == "--port") {
			const auto number = parse_number(take_value());
			if (not number or *number == 0 or *number > 65535) {
				fail(program, "relay port must be between 1 and 65535");
			}
			port = static_cast<uint16_t>(*number);
		}
		else if (name == "--player-token") {
			const auto token = parse_token(take_value());
			if (not token) {
				fail(program, "relay player tokens must be 64 hexadecimal characters and not all zero");
			}
			tokens.push_back(*token);
		}
		else if (name == "--max-rooms") {
			const auto number = parse_number(take_value());
			if (not number or *number == 0 or *number > 4096) {
				fail(program, "relay max rooms must be between 1 and 4096");
			}
			max_rooms = static_cast<size_t>(*number);
		}
		else if (name == "--max-connections") {
			const auto number = parse_number(take_value());
			if (not number or *number == 0) {
				fail(program, "relay max connections must be positive");
			}
			max_connections = static_cast<size_t>(*number);
		}
		else {
			fail(program, "unknown option " + std::string{arg});
		}
	}

	if (std::set<std::string>{tokens.begin(), tokens.end()}.size() != tokens.size()) {
		fail(program, "relay player tokens must be unique");
	}

	// Handle Ctrl+C / SIGTERM on a dedicated thread: stop_relay_service()
	// takes a lock, so it must not be called from a signal handler.
	sigset_t stop_signals;
	sigemptyset(&stop_signals);
	sigaddset(&stop_signals, SIGINT);
	sigaddset(&stop_signals, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &stop_signals, nullptr);

	std::thread signal_thread{[stop_signals]() {
		int received = 0;
		if (sigwait(&stop_signals, &received) == 0) {
			openage::network::stop_relay_service();
		}
	}};
	signal_thread.detach();

	// Register the stdout log sink before the first message: messages are
	// only formatted when a sink exists, so the first one would print empty.
	openage::log::global_stdoutsink();

	std::cout << "starting FVD room relay with "
	          << (tokens.empty() ? std::string{"public access"}
	                             : std::to_string(tokens.size()) + " authorized player token(s)")
	          << std::endl;

	try {
		openage::network::run_relay_service(listen, port, tokens, max_rooms, max_connections);
	}
	catch (const std::exception &error) {
		std::cerr << program << ": " << error.what() << std::endl;
		return 1;
	}

	std::cout << "FVD room relay stopped" << std::endl;
	return 0;
}
