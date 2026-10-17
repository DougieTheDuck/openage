// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include "network/bounded_mailbox.h"
#include "network/messages.h"
#include "network/world_delta.h"
#include "network/session.h"

namespace openage::network {

/** Complete state sent to one connection. */
struct WorldSnapshotMessage {
	WorldState state{};
};

/** Incremental state sent to one connection. */
struct WorldDeltaMessage {
	WorldDelta state{};
};

/** Typed simulation output waiting for one connection thread. */
using OutboundMessage = std::variant<
	CommandResult,
	WorldSnapshotMessage,
	WorldDeltaMessage,
	ParticipantDisconnected>;

/** Bounded simulation-to-connection mailbox for one accepted session. */
class OutboundMailbox {
public:
	/** Create a mailbox with room for at most max_pending messages. */
	explicit OutboundMailbox(size_t max_pending);

	/** Attempt to enqueue one message without blocking. */
	bool try_push(OutboundMessage message);

	/** Remove up to maximum_count messages in FIFO arrival order. */
	std::vector<OutboundMessage> drain(size_t maximum_count);

	/** Number of messages waiting for the connection thread. */
	size_t size() const;

	/** Maximum number of pending messages. */
	size_t capacity() const;

private:
	BoundedMailbox<OutboundMessage> messages;
};

/** Accepted connection closure waiting for authoritative handling. */
struct ConnectionClosed {
	SessionIdentity identity{};
	DisconnectReason reason{DisconnectReason::CONNECTION_LOST};
};

/** Bounded connection-to-simulation mailbox for lifecycle events. */
class ConnectionEventMailbox {
public:
	/** Create a mailbox with room for at most max_pending events. */
	explicit ConnectionEventMailbox(size_t max_pending);

	/** Attempt to enqueue one validated closure without blocking. */
	bool try_push(ConnectionClosed closed);

	/** Remove up to maximum_count events in FIFO arrival order. */
	std::vector<ConnectionClosed> drain(size_t maximum_count);

	/** Number of events waiting for authoritative handling. */
	size_t size() const;

private:
	BoundedMailbox<ConnectionClosed> events;
};

/** Encode one typed simulation message using the connection-owned session. */
Frame make_outbound_frame(ConnectionSession &session,
	                      const OutboundMessage &message);

} // namespace openage::network
