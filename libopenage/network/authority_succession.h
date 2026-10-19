// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gamestate/types.h"
#include "network/messages.h"

namespace openage::network {

/**
 * An epoch fences old authoritative traffic from newly elected authorities.
 * A player identifier alone is not sufficient: player 1 may become the
 * authority after the original lobby creator (an assigned Builder) exits.
 */
struct AuthorityLease {
	uint64_t epoch{0};
	gamestate::player_id_t authority{0};
	uint64_t checkpoint_revision{0};
};

enum class AuthorityLossAction {
	CONTINUE,
	END_MATCH,
	REQUIRE_RESTORED_CHECKPOINT,
};

/**
 * Role-based authority succession plan for an immutable StartMatch.
 *
 * This is the *eligibility and split-brain guard*, not a host migration
 * implementation. A WorldState is a presentation snapshot and CANNOT prove
 * that the live event loop, curves, queued commands, RNG and construction
 * timers were restored. The caller must supply a successful restoration
 * attestation from a complete simulation checkpoint before commit().
 *
 * No current production caller can supply that attestation yet, so a lost
 * authoritative Builder correctly ends the match rather than guessing.
 */
class AuthoritySuccession {
public:
	AuthoritySuccession(const StartMatch &assignment,
	                    uint64_t original_host_participant_id);

	/** Current role assignment; Hunter is always player 1. */
	gamestate::player_id_t hunter_player() const;
	gamestate::player_id_t authority_player() const;
	const AuthorityLease &lease() const;
	bool host_is_builder() const;

	/** Enforce the Hunter-only match-ending rule on player loss. */
	AuthorityLossAction disconnected(gamestate::player_id_t player);

	/**
	 * A complete simulation restore is REQUIRED before preparing takeover.
	 * The checkpoint must represent the authoritative epoch at a finalized,
	 * positive world revision. false is never considered a valid restore.
	 */
	void attest_restored_checkpoint(uint64_t source_epoch,
	                                uint64_t restored_world_revision,
	                                bool full_simulation_restored);

	/**
	 * Finalize a single Hunter takeover after a valid restore. The caller
	 * must establish a new transport session for the Hunter and reconnect
	 * surviving Builders; legacy session sequence numbers are never reused.
	 */
	AuthorityLease commit_hunter_takeover();

	/** Reject stale commands, deltas and duplicated takeover commits. */
	bool accepts_authority_epoch(uint64_t epoch,
	                             gamestate::player_id_t claimed_authority) const;

	/** Replay protection for participants that have acknowledged a new lease. */
	bool acknowledge_lease(gamestate::player_id_t player, uint64_t epoch);
	bool has_acknowledged(gamestate::player_id_t player) const;

private:
	std::vector<gamestate::player_id_t> participants{};
	AuthorityLease current{};
	bool hunter_connected{true};
	bool original_host_lost{false};
	std::optional<uint64_t> restored_revision{};
	std::vector<gamestate::player_id_t> acknowledgements{};
};

} // namespace openage::network
