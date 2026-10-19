// Copyright 2026 the openage authors. See copying.md for legal info.

#include "authority_succession.h"

#include <cstdint>
#include <limits>

#include "network/lobby.h"
#include "testing/testing.h"

namespace openage::network::tests {

void authority_succession() {
	// Preserve the weighted Hunter lottery: the lobby host is a Builder,
	// but an admitted remote guest owns Hunter/player 1.
	HostLobby lobby{11, "Builder Host"};
	lobby.join(22, "Hunter Guest");
	lobby.join(33, "Surviving Builder");
	auto assignment = lobby.start([](uint64_t bound) {
		return bound == 5 ? uint64_t{1} : uint64_t{0};
	});
	TESTEQUALS(player_for_participant(assignment, 22), gamestate::player_id_t{1});
	const auto original_authority = player_for_participant(assignment, 11);
	TESTEQUALS(original_authority != 1, true);
	AuthoritySuccession succession{assignment, 11};
	TESTEQUALS(succession.hunter_player(), gamestate::player_id_t{1});
	TESTEQUALS(succession.authority_player(), original_authority);
	TESTEQUALS(succession.lease().epoch, uint64_t{1});
	TESTEQUALS(succession.host_is_builder(), true);
	TESTEQUALS(succession.accepts_authority_epoch(1, original_authority), true);
	TESTEQUALS(succession.accepts_authority_epoch(2, original_authority), false);
	TESTEQUALS(succession.accepts_authority_epoch(1, 1), false);
	TESTEQUALS(succession.acknowledge_lease(1, 1), true);
	TESTEQUALS(succession.acknowledge_lease(1, 1), false);

	// An unrelated Builder can depart without affecting the authority.
	const auto builder = player_for_participant(assignment, 33);
	TESTEQUALS(succession.disconnected(builder) == AuthorityLossAction::CONTINUE, true);
	TESTEQUALS(succession.accepts_authority_epoch(1, original_authority), true);
	TESTEQUALS(succession.acknowledge_lease(builder, 1), false);
	TESTTHROWS(succession.disconnected(builder));

	// When the authoritative Builder leaves, no presentation WorldState can
	// be promoted into a simulation. Epoch 1 becomes fenced immediately.
	TESTEQUALS(succession.disconnected(original_authority)
	           == AuthorityLossAction::REQUIRE_RESTORED_CHECKPOINT, true);
	TESTEQUALS(succession.accepts_authority_epoch(1, original_authority), false);
	TESTEQUALS(succession.accepts_authority_epoch(2, 1), false);
	TESTTHROWS(succession.commit_hunter_takeover());
	TESTTHROWS(succession.attest_restored_checkpoint(1, 12, false));
	TESTTHROWS(succession.attest_restored_checkpoint(0, 12, true));
	TESTTHROWS(succession.attest_restored_checkpoint(1, 0, true));
	TESTEQUALS(succession.has_acknowledged(1), true); // Old acknowledgement is not a new lease.
	// Synthetic full-restoration attestation exercises the epoch transition.
	// The real engine does not yet have a full checkpoint serializer/restore.
	succession.attest_restored_checkpoint(1, 12, true);
	TESTTHROWS(succession.attest_restored_checkpoint(1, 11, true));
	auto new_lease = succession.commit_hunter_takeover();
	TESTEQUALS(new_lease.epoch, uint64_t{2});
	TESTEQUALS(new_lease.authority, gamestate::player_id_t{1});
	TESTEQUALS(new_lease.checkpoint_revision, uint64_t{12});
	TESTEQUALS(succession.host_is_builder(), false);
	TESTEQUALS(succession.accepts_authority_epoch(1, original_authority), false);
	TESTEQUALS(succession.accepts_authority_epoch(2, 1), true);
	TESTEQUALS(succession.has_acknowledged(1), false);
	TESTEQUALS(succession.acknowledge_lease(1, 1), false);
	TESTEQUALS(succession.acknowledge_lease(1, 2), true);
	TESTTHROWS(succession.commit_hunter_takeover());

	// Preferred pre-game route: before the authoritative simulation has
	// started, everyone joins a Hunter-owned room with *new* authenticated
	// sessions. It must not alter the already drawn player roles.
	AuthoritySuccession pregame{assignment, 11};
	TESTEQUALS(pregame.pregame_rehost_ready(), false);
	TESTTHROWS(pregame.commit_pregame_hunter_rehost());
	TESTEQUALS(pregame.acknowledge_pregame_rehost({101, 1}, 1), false);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({0, 1}, 2), false);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({201, 8}, 2), false);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({101, 1}, 2), true);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({101, original_authority}, 2), false);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({101, 1}, 2), false);
	TESTEQUALS(pregame.acknowledge_pregame_rehost({102, original_authority}, 2), true);
	TESTEQUALS(pregame.pregame_rehost_ready(), false);
	TESTTHROWS(pregame.commit_pregame_hunter_rehost());
	TESTEQUALS(pregame.acknowledge_pregame_rehost({103, builder}, 2), true);
	TESTEQUALS(pregame.pregame_rehost_ready(), true);
	auto pregame_lease = pregame.commit_pregame_hunter_rehost();
	TESTEQUALS(pregame_lease.epoch, uint64_t{2});
	TESTEQUALS(pregame_lease.authority, gamestate::player_id_t{1});
	TESTEQUALS(pregame_lease.checkpoint_revision, uint64_t{0});
	TESTEQUALS(pregame.accepts_authority_epoch(1, original_authority), false);
	TESTEQUALS(pregame.accepts_authority_epoch(2, 1), true);
	TESTTHROWS(pregame.commit_pregame_hunter_rehost());
	TESTEQUALS(pregame.disconnected(original_authority)
	           == AuthorityLossAction::CONTINUE, true);
	TESTEQUALS(pregame.accepts_authority_epoch(2, 1), true);
	pregame.mark_bootstrap_started();
	TESTEQUALS(pregame.acknowledge_pregame_rehost({500, 1}, 3), false);

	AuthoritySuccession too_late{assignment, 11};
	too_late.mark_bootstrap_started();
	TESTEQUALS(too_late.acknowledge_pregame_rehost({101, 1}, 2), false);
	TESTTHROWS(too_late.commit_pregame_hunter_rehost());

	// Losing the Hunter ends play after succession as well.
	TESTEQUALS(succession.disconnected(1) == AuthorityLossAction::END_MATCH, true);
	TESTEQUALS(succession.accepts_authority_epoch(2, 1), false);
	TESTEQUALS(succession.acknowledge_lease(1, 2), false);

	// A lobby with the Hunter as original host never has a Builder takeover.
	HostLobby hunter_lobby{101, "Hunter Host"};
	hunter_lobby.join(102, "Builder Guest");
	auto hunter_start = hunter_lobby.start([](uint64_t) { return uint64_t{0}; });
	AuthoritySuccession hunter_host{hunter_start, 101};
	TESTEQUALS(hunter_host.host_is_builder(), false);
	TESTEQUALS(hunter_host.disconnected(2) == AuthorityLossAction::CONTINUE, true);
	TESTTHROWS(hunter_host.commit_hunter_takeover());
	TESTEQUALS(hunter_host.disconnected(1) == AuthorityLossAction::END_MATCH, true);
	TESTTHROWS(hunter_host.attest_restored_checkpoint(1, 2, true));

	// Avoid constructing a succession record with a forged lobby creator.
	TESTTHROWS(AuthoritySuccession(assignment, 22));
}

} // namespace openage::network::tests
