// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_service.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "error/error.h"
#include "log/log.h"
#include "log/message.h"
#include "network/relay.h"

namespace openage::network {

namespace {

std::mutex running_listener_mutex{};
TcpListener *running_listener{};

class RunningRelayListener {
public:
	explicit RunningRelayListener(TcpListener &listener) :
		listener{&listener} {
		std::scoped_lock lock{running_listener_mutex};
		if (running_listener != nullptr) {
			throw Error{MSG(err) << "Only one command-line relay can run in a process"};
		}
		running_listener = this->listener;
	}

	~RunningRelayListener() {
		std::scoped_lock lock{running_listener_mutex};
		if (running_listener == this->listener) {
			running_listener = nullptr;
		}
	}

	RunningRelayListener(const RunningRelayListener &) = delete;
	RunningRelayListener &operator=(const RunningRelayListener &) = delete;

private:
	TcpListener *listener;
};

RelayToken service_token(const std::string &token) {
	if (token.size() != RELAY_TOKEN_SIZE) {
		throw Error{MSG(err) << "FVD relay service tokens must contain exactly "
		                     << RELAY_TOKEN_SIZE << " bytes"};
	}

	RelayToken result{};
	std::transform(token.begin(), token.end(), result.begin(), [](char value) {
		return static_cast<uint8_t>(static_cast<unsigned char>(value));
	});
	return result;
}

struct RelayWorker {
	std::shared_ptr<std::atomic_bool> finished;
	std::jthread thread;
};

} // namespace

void serve_relay_directory(TcpListener &listener,
                           RelayRoomDirectory &directory,
                           size_t max_connections) {
	if (max_connections == 0) {
		throw Error{MSG(err) << "FVD relay service connection capacity must be nonzero"};
	}

	std::mutex connections_mutex{};
	std::vector<std::shared_ptr<TcpConnection>> active_connections{};
	std::atomic_bool stopping{};
	std::mutex pairs_mutex{};
	std::vector<TcpConnection *> active_pair_connections{};
	std::vector<RelayWorker> workers{};

	auto remove_finished = [&]() {
		workers.erase(
			std::remove_if(
				workers.begin(),
				workers.end(),
				[](RelayWorker &worker) {
					if (not worker.finished->load()) {
						return false;
					}
					worker.thread.join();
					return true;
				}),
			workers.end());
	};

	while (listener.is_open()) {
		remove_finished();
		std::shared_ptr<TcpConnection> connection{};
		try {
			connection = std::make_shared<TcpConnection>(listener.accept());
		}
		catch (...) {
			if (not listener.is_open()) {
				break;
			}
			throw;
		}

		{
			std::scoped_lock lock{connections_mutex};
			if (active_connections.size() >= max_connections) {
				reject_relay(*connection, "relay connection capacity is full");
				continue;
			}
			active_connections.push_back(connection);
		}

		auto finished = std::make_shared<std::atomic_bool>(false);
		workers.push_back({
			finished,
			std::jthread{[&, connection, finished]() {
				try {
					if (auto pair = directory.admit(*connection)) {
						bool forward{};
						{
							std::scoped_lock lock{pairs_mutex};
							if (stopping.load()) {
								pair->host.close();
								pair->guest.close();
							}
							else {
								active_pair_connections.push_back(&pair->host);
								active_pair_connections.push_back(&pair->guest);
								forward = true;
							}
						}
						if (forward) {
							auto unregister_pair = [&]() {
								std::scoped_lock lock{pairs_mutex};
								for (auto *peer : {&pair->host, &pair->guest}) {
									active_pair_connections.erase(
										std::remove(active_pair_connections.begin(), active_pair_connections.end(), peer),
										active_pair_connections.end());
								}
							};
							try {
								forward_relay_pair(pair->host, pair->guest);
							}
							catch (...) {
								unregister_pair();
								throw;
							}
							unregister_pair();
						}
					}
				}
				catch (const std::exception &error) {
					log::log(WARN << "FVD relay connection ended with an error: "
					              << error.what());
				}

				{
					std::scoped_lock lock{connections_mutex};
					active_connections.erase(
						std::remove(active_connections.begin(), active_connections.end(), connection),
						active_connections.end());
				}
				finished->store(true);
			}}});
	}

	stopping.store(true);
	directory.cancel_all();
	{
		std::scoped_lock lock{connections_mutex};
		for (const auto &connection : active_connections) {
			connection->close();
		}
	}
	{
		std::scoped_lock lock{pairs_mutex};
		for (auto *connection : active_pair_connections) {
			connection->close();
		}
	}
	for (auto &worker : workers) {
		worker.thread.join();
	}
}

void run_relay_service(
	const std::string &address,
	uint16_t port,
	const std::vector<std::string> &authorized_tokens,
	size_t max_rooms,
	size_t max_connections) {
	if (port == 0) {
		throw Error{MSG(err) << "FVD relay service requires a nonzero listening port"};
	}

	std::vector<RelayToken> tokens{};
	tokens.reserve(authorized_tokens.size());
	for (const auto &token : authorized_tokens) {
		tokens.push_back(service_token(token));
	}
	RelayRoomDirectory directory{max_rooms, std::move(tokens)};
	auto listener = TcpListener::listen(address, port);
	RunningRelayListener running{listener};
	log::log(INFO << "FVD relay listening on "
	              << (address.empty() ? std::string{"all interfaces"} : address)
	              << ':' << listener.get_port()
	              << (directory.requires_token() ? " with player-token access" : " with public access"));
	serve_relay_directory(listener, directory, max_connections);
}

void stop_relay_service() noexcept {
	std::scoped_lock lock{running_listener_mutex};
	if (running_listener != nullptr) {
		running_listener->close();
	}
}

} // namespace openage::network
