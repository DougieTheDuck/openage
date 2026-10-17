// Copyright 2026 the openage authors. See copying.md for legal info.

#include "relay_service.h"

#include <algorithm>
#include <atomic>
#include <exception>
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

RelayPairCredentials service_credentials(const RelayServiceCredentials &credentials) {
	return {
		credentials.match_id,
		credentials.player_id,
		service_token(credentials.host_token),
		service_token(credentials.guest_token),
	};
}

} // namespace

void serve_relay_pairs(TcpListener &listener,
                       RelayPairRegistry &registry,
                       size_t pair_count) {
	if (pair_count == 0 or registry.pair_count() != pair_count) {
		throw Error{MSG(err) << "FVD relay service requires every expected pair "
		                     << "to be provisioned before admission"};
	}

	std::atomic_size_t completed_pairs{};
	std::atomic_bool stopping{};
	std::mutex admissions_mutex{};
	std::vector<std::shared_ptr<TcpConnection>> active_admissions{};
	std::mutex error_mutex{};
	std::exception_ptr service_error{};
	std::vector<std::jthread> workers{};

	auto close_admissions = [&]() {
		std::scoped_lock lock{admissions_mutex};
		for (const auto &connection : active_admissions) {
			connection->close();
		}
	};
	auto fail_admission = [&](std::exception_ptr error) {
		{
			std::scoped_lock lock{error_mutex};
			if (not service_error) {
				service_error = error;
			}
		}
		stopping.store(true);
		listener.close();
		registry.cancel_all();
		close_admissions();
	};

	while (not stopping.load()) {
		std::shared_ptr<TcpConnection> connection{};
		try {
			connection = std::make_shared<TcpConnection>(listener.accept());
		}
		catch (...) {
			if (stopping.load()) {
				break;
			}
			throw;
		}
		{
			std::scoped_lock lock{admissions_mutex};
			if (stopping.load()) {
				connection->close();
				break;
			}
			active_admissions.push_back(connection);
		}

		workers.emplace_back([&, connection]() {
			std::optional<RelayPair> pair{};
			try {
				pair = registry.admit(*connection);
			}
			catch (...) {
				fail_admission(std::current_exception());
			}

			if (pair.has_value()) {
				auto completed = completed_pairs.fetch_add(1) + 1;
				if (completed == pair_count) {
					stopping.store(true);
					listener.close();
					close_admissions();
				}
				try {
					forward_relay_pair(std::move(*pair));
				}
				catch (...) {
					std::scoped_lock lock{error_mutex};
					if (not service_error) {
						service_error = std::current_exception();
					}
				}
			}

			std::scoped_lock lock{admissions_mutex};
			active_admissions.erase(
				std::remove(active_admissions.begin(), active_admissions.end(), connection),
				active_admissions.end());
		});
	}

	for (auto &worker : workers) {
		worker.join();
	}
	if (service_error) {
		std::rethrow_exception(service_error);
	}
}

void run_relay_service(
	const std::string &address,
	uint16_t port,
	const std::vector<RelayServiceCredentials> &credentials) {
	if (credentials.empty()) {
		throw Error{MSG(err) << "FVD relay service requires at least one pair"};
	}
	if (port == 0) {
		throw Error{MSG(err) << "FVD relay service requires a nonzero listening port"};
	}

	RelayPairRegistry registry{credentials.size()};
	for (const auto &pair : credentials) {
		registry.provision(service_credentials(pair));
	}

	auto listener = TcpListener::listen(address, port);
	log::log(INFO << "FVD relay listening on "
	              << (address.empty() ? std::string{"all interfaces"} : address)
	              << ':' << listener.get_port()
	              << " for " << credentials.size() << " guest pair(s)");
	serve_relay_pairs(listener, registry, credentials.size());
}

} // namespace openage::network
