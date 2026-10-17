// Copyright 2026 the openage authors. See copying.md for legal info.

#include "replication.h"

#include "testing/testing.h"

namespace openage::network::tests {

void replication() {
	ReplicationTracker tracker{2};
	TESTEQUALS(tracker.get_local_player_id(), gamestate::player_id_t{2});
	not tracker.get_local_kills().has_value() or TESTFAIL;
	TESTTHROWS(ReplicationTracker{0});
	TESTTHROWS(ReplicationTracker{9});
	tracker.is_host_connected() or TESTFAIL;
	not tracker.has_state() or TESTFAIL;
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{0});

	WorldState delta_before_snapshot{};
	delta_before_snapshot.base_revision = 0;
	delta_before_snapshot.world_revision = 1;
	delta_before_snapshot.local_player_id = 2;
	delta_before_snapshot.local_kill_count = 99;
	tracker.accept_delta(delta_before_snapshot)
		== DeltaDisposition::REQUEST_SNAPSHOT
		or TESTFAIL;
	tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.make_snapshot_request().last_applied_revision, uint64_t{0});
	tracker.accept_delta(delta_before_snapshot)
		== DeltaDisposition::AWAIT_SNAPSHOT
		or TESTFAIL;

	WorldState initial_snapshot{};
	initial_snapshot.world_revision = 4;
	initial_snapshot.local_player_id = 2;
	initial_snapshot.local_kill_count = 1;
	auto foreign_snapshot = initial_snapshot;
	foreign_snapshot.local_player_id = 3;
	TESTTHROWS(tracker.accept_snapshot(foreign_snapshot));
	not tracker.get_local_kills().has_value() or TESTFAIL;
	tracker.accept_snapshot(initial_snapshot);
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{1});
	tracker.has_state() or TESTFAIL;
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{4});

	WorldState contiguous{};
	contiguous.base_revision = 4;
	contiguous.world_revision = 6;
	contiguous.local_player_id = 2;
	contiguous.local_kill_count = 3;
	auto foreign_delta = contiguous;
	foreign_delta.local_player_id = 3;
	TESTTHROWS(tracker.accept_delta(foreign_delta));
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{1});
	TESTEQUALS(tracker.get_revision(), uint64_t{4});
	tracker.accept_delta(contiguous) == DeltaDisposition::APPLY or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{6});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{3});

	WorldState invalid{};
	invalid.base_revision = 6;
	invalid.world_revision = 6;
	invalid.local_player_id = 2;
	invalid.local_kill_count = 9;
	TESTTHROWS(tracker.accept_delta(invalid));
	TESTEQUALS(tracker.get_revision(), uint64_t{6});

	WorldState gap{};
	gap.base_revision = 7;
	gap.world_revision = 8;
	gap.local_player_id = 2;
	gap.local_kill_count = 11;
	tracker.accept_delta(gap) == DeltaDisposition::REQUEST_SNAPSHOT or TESTFAIL;
	tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.make_snapshot_request().last_applied_revision, uint64_t{6});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{3});

	WorldState stale_snapshot{};
	stale_snapshot.world_revision = 5;
	TESTTHROWS(tracker.accept_snapshot(stale_snapshot));
	tracker.is_awaiting_snapshot() or TESTFAIL;

	WorldState recovery_snapshot{};
	recovery_snapshot.world_revision = 9;
	recovery_snapshot.local_player_id = 2;
	recovery_snapshot.local_kill_count = 7;
	tracker.accept_snapshot(recovery_snapshot);
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{9});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{7});

	auto invalid_snapshot = recovery_snapshot;
	invalid_snapshot.base_revision = 1;
	TESTTHROWS(tracker.accept_snapshot(invalid_snapshot));

	// A lost authoritative host ends this match. No later frames or
	// snapshot requests may revive an independent client simulation.
	tracker.host_disconnected();
	TESTEQUALS(tracker.is_host_connected(), false);
	TESTEQUALS(tracker.has_state(), false);
	TESTEQUALS(tracker.is_awaiting_snapshot(), false);
	not tracker.get_local_kills().has_value() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{9});
	TESTTHROWS(tracker.accept_snapshot(recovery_snapshot));
	TESTTHROWS(tracker.accept_delta(contiguous));
	TESTTHROWS(tracker.make_snapshot_request());

	// Different recipients receive independent authoritative totals.
	ReplicationTracker other_player{3};
	auto other_snapshot = recovery_snapshot;
	other_snapshot.local_player_id = 3;
	other_snapshot.local_kill_count = 12;
	other_player.accept_snapshot(other_snapshot);
	TESTEQUALS(other_player.get_local_kills().value(), uint64_t{12});
	not tracker.get_local_kills().has_value() or TESTFAIL;
}

} // namespace openage::network::tests
