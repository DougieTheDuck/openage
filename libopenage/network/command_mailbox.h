// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

#include "network/messages.h"
#include "network/session.h"

namespace openage::network {

/** Command paired with identity established by its accepted connection. */
struct AuthenticatedCommand {
	SessionIdentity identity{};
	CommandIntent intent{};
};

/**
 * Bounded multi-producer mailbox from connection threads to simulation.
 *
 * Connection threads validate and decode complete frames before enqueueing.
 * The simulation thread drains commands in arrival order and applies them at
 * an authoritative simulation time.
 */
class CommandMailbox {
public:
	/** Create a mailbox with room for at most max_pending commands. */
	explicit CommandMailbox(size_t max_pending);

	/**
	 * Attempt to enqueue one authenticated command without blocking.
	 *
	 * @return false when the mailbox is full. The caller must send a visible
	 * rejection or disconnect the peer rather than silently dropping input.
	 */
	bool try_push(AuthenticatedCommand command);

	/** Remove up to maximum_count commands in FIFO arrival order. */
	std::vector<AuthenticatedCommand> drain(size_t maximum_count);

	/** Number of commands waiting for the simulation thread. */
	size_t size() const;

	/** Maximum number of pending commands. */
	size_t capacity() const;

private:
	const size_t max_pending;
	mutable std::mutex mutex;
	std::deque<AuthenticatedCommand> commands;
};

} // namespace openage::network
