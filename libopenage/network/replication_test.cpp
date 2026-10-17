// Copyright 2026 the openage authors. See copying.md for legal info.

#include "replication.h"

#include "testing/testing.h"

namespace openage::network::tests {

void replication() {
	ReplicationTracker tracker{};
	tracker.is_host_connected() or TESTFAIL;
	not tracker.has_state() or TESTFAIL;
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{0});

	WorldState delta_before_snapshot{};
	delta_before_snapshot.base_revision = 0;
	delta_before_snapshot.world_revision = 1;
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
	tracker.accept_snapshot(initial_snapshot);
	tracker.has_state() or TESTFAIL;
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{4});

	WorldState contiguous{};
	contiguous.base_revision = 4;
	contiguous.world_revision = 6;
	tracker.accept_delta(contiguous) == DeltaDisposition::APPLY or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{6});

	WorldState invalid{};
	invalid.base_revision = 6;
	invalid.world_revision = 6;
	TESTTHROWS(tracker.accept_delta(invalid));
	TESTEQUALS(tracker.get_revision(), uint64_t{6});

	WorldState gap{};
	gap.base_revision = 7;
	gap.world_revision = 8;
	tracker.accept_delta(gap) == DeltaDisposition::REQUEST_SNAPSHOT or TESTFAIL;
	tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.make_snapshot_request().last_applied_revision, uint64_t{6});

	WorldState stale_snapshot{};
	stale_snapshot.world_revision = 5;
	TESTTHROWS(tracker.accept_snapshot(stale_snapshot));
	tracker.is_awaiting_snapshot() or TESTFAIL;

	WorldState recovery_snapshot{};
	recovery_snapshot.world_revision = 9;
	tracker.accept_snapshot(recovery_snapshot);
	not tracker.is_awaiting_snapshot() or TESTFAIL;
	TESTEQUALS(tracker.get_revision(), uint64_t{9});

	auto invalid_snapshot = recovery_snapshot;
	invalid_snapshot.base_revision = 1;
	TESTTHROWS(tracker.accept_snapshot(invalid_snapshot));

	// A lost authoritative host ends this match. No later frames or
	// snapshot requests may revive an independent client simulation.
	tracker.host_disconnected();
	TESTEQUALS(tracker.is_host_connected(), false);
	TESTEQUALS(tracker.has_state(), false);
	TESTEQUALS(tracker.is_awaiting_snapshot(), false);
	TESTEQUALS(tracker.get_revision(), uint64_t{9});
	TESTTHROWS(tracker.accept_snapshot(recovery_snapshot));
	TESTTHROWS(tracker.accept_delta(contiguous));
	TESTTHROWS(tracker.make_snapshot_request());
}

} // namespace openage::network::tests
