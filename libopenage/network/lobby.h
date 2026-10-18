// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "network/messages.h"

namespace openage::network {

/**
 * Host-owned pre-match lobby, separate from game entities and fixed player slots.
 *
 * All mutations and the Hunter lottery run on the host's lobby thread.
 * Assignments become immutable when start() succeeds.
 */
class HostLobby {
public:
	using draw_t = std::function<uint64_t(uint64_t exclusive_upper_bound)>;

	explicit HostLobby(uint64_t host_id, std::string host_name);

	/** Add a guest with a stable, host-generated participant identity. */
	void join(uint64_t participant_id, std::string name);

	/** Remove a departing guest. The host cannot leave without closing the lobby. */
	void leave(uint64_t participant_id);

	/** Broadcastable view of current participants, in admission order. */
	ParticipantSnapshot snapshot() const;

	/**
	 * Start with 2-8 people. The host has one Hunter lottery ticket, each
	 * guest two. Other participants are randomly assigned Builder slots 2..N.
	 *
	 * draw(exclusive_upper_bound) must return an unbiased value in
	 * [0, exclusive_upper_bound), and can be injected for deterministic tests.
	 * No lobby state changes if the callback throws or returns a bad value.
	 */
	StartMatch start(const draw_t &draw);

	/** Production variant using an unbiased, host-owned random generator. */
	StartMatch start();

	bool started() const;
	size_t size() const;

private:
	ParticipantSnapshot current{};
	bool match_started{false};
};

/** Validate a complete snapshot or final start assignment, including IDs. */
void validate_participant_snapshot(const ParticipantSnapshot &snapshot);
void validate_start_match(const StartMatch &start);

/** Ensure StartMatch exactly matches the observed roster, not a substituted guest. */
void validate_start_match_against_roster(const ParticipantSnapshot &roster,
                                         const StartMatch &start);

/** Resolve a previously validated participant identity to its game slot. */
gamestate::player_id_t player_for_participant(const StartMatch &start,
                                               uint64_t participant_id);

} // namespace openage::network
