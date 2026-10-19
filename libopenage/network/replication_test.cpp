// Copyright 2026 the openage authors. See copying.md for legal info.

#include "replication.h"

#include "testing/testing.h"

namespace openage::network::tests {

void replication() {
	ReplicationTracker tracker{2};
	TESTEQUALS(tracker.get_local_player_id(), gamestate::player_id_t{2});
	TESTEQUALS(tracker.get_local_kills().has_value(), false);
	TESTEQUALS(tracker.get_tower_kills(100).has_value(), false);
	TESTEQUALS(tracker.get_world_state().has_value(), false);
	TESTEQUALS(tracker.get_presentation_state().has_value(), false);
	TESTTHROWS(ReplicationTracker{0});
	TESTTHROWS(ReplicationTracker{9});
	TESTEQUALS(tracker.is_host_connected(), true);
	TESTEQUALS(tracker.has_state(), false);
	TESTEQUALS(tracker.get_revision(), uint64_t{0});

	WorldState initial{};
	initial.map_size = 128;
	initial.local_player_id = 2;
	initial.world_revision = 4;
	initial.local_kill_count = 1;
	initial.players = {{1, PlayerRole::HUNTER}, {2, PlayerRole::BUILDER},
	                   {3, PlayerRole::BUILDER}};
	initial.entities = {
		{100, 2, "fvd.data.entities.Tower", {1, 1, 0}, 100, 100, 1},
		{200, 3, "fvd.data.entities.Tower", {2, 2, 0}, 100, 100},
	};
	auto foreign_snapshot = initial;
	foreign_snapshot.local_player_id = 3;
	TESTTHROWS(tracker.accept_snapshot(foreign_snapshot));
	TESTEQUALS(tracker.get_local_kills().has_value(), false);
	tracker.accept_snapshot(initial);
	const auto initial_generation = tracker.get_presentation_state()->snapshot_generation;
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{1});
	TESTEQUALS(tracker.get_tower_kills(100).value(), uint64_t{1});
	TESTEQUALS(tracker.get_tower_kills(200).has_value(), false);
	TESTEQUALS(tracker.has_state(), true);
	TESTEQUALS(tracker.is_awaiting_snapshot(), false);
	TESTEQUALS(tracker.get_revision(), uint64_t{4});

	WorldState contiguous = initial;
	contiguous.world_revision = 6;
	contiguous.local_kill_count = 3;
	contiguous.entities = {
		{100, 2, "fvd.data.entities.Tower", {1, 1, 0}, 100, 100, 2},
		{101, 2, "fvd.data.entities.Tower", {3, 3, 0}, 100, 100, 1},
	};
	auto delta = make_world_delta(initial, contiguous);
	auto foreign = delta;
	foreign.changed.local_player_id = 3;
	TESTTHROWS(tracker.accept_delta(foreign));
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{1});
	TESTEQUALS(tracker.get_revision(), uint64_t{4});
	TESTEQUALS(tracker.accept_delta(delta) == DeltaDisposition::APPLY, true);
	TESTEQUALS(tracker.get_revision(), uint64_t{6});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{3});
	TESTEQUALS(tracker.get_tower_kills(100).value(), uint64_t{2});
	TESTEQUALS(tracker.get_tower_kills(101).value(), uint64_t{1});
	TESTEQUALS(tracker.get_tower_kills(200).has_value(), false);
	TESTEQUALS(tracker.get_world_state()->entities.size(), size_t{2});
	TESTEQUALS(tracker.get_presentation_state()->snapshot_generation, initial_generation);

	auto invalid = delta;
	invalid.changed.base_revision = 6;
	invalid.changed.world_revision = 6;
	TESTTHROWS(tracker.accept_delta(invalid));
	TESTEQUALS(tracker.get_revision(), uint64_t{6});

	WorldState future = contiguous;
	future.world_revision = 8;
	future.local_kill_count = 11;
	auto gap = make_world_delta(contiguous, future);
	gap.changed.base_revision = 7;
	TESTEQUALS(tracker.accept_delta(gap) == DeltaDisposition::REQUEST_SNAPSHOT, true);
	TESTEQUALS(tracker.is_awaiting_snapshot(), true);
	TESTEQUALS(tracker.make_snapshot_request().last_applied_revision, uint64_t{6});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{3});
	TESTEQUALS(tracker.get_tower_kills(100).value(), uint64_t{2});
	TESTEQUALS(tracker.accept_delta(gap) == DeltaDisposition::AWAIT_SNAPSHOT, true);

	auto stale = initial;
	stale.world_revision = 5;
	TESTTHROWS(tracker.accept_snapshot(stale));
	TESTEQUALS(tracker.is_awaiting_snapshot(), true);

	WorldState recovered = future;
	recovered.world_revision = 9;
	recovered.local_kill_count = 7;
	recovered.entities = {
		{101, 2, "fvd.data.entities.Tower", {3, 3, 0}, 100, 100, 4},
	};
	tracker.accept_snapshot(recovered);
	TESTEQUALS(tracker.get_presentation_state()->snapshot_generation, initial_generation + 1);
	TESTEQUALS(tracker.is_awaiting_snapshot(), false);
	TESTEQUALS(tracker.get_revision(), uint64_t{9});
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{7});
	TESTEQUALS(tracker.get_tower_kills(100).has_value(), false);
	TESTEQUALS(tracker.get_tower_kills(101).value(), uint64_t{4});
	TESTEQUALS(tracker.get_world_state()->entities.size(), size_t{1});

	// Bad removal must not partly mutate the client's reconstructed world.
	auto no_change = recovered;
	no_change.world_revision = 10;
	auto corrupt = make_world_delta(recovered, no_change);
	corrupt.removed_entities = {9999};
	TESTTHROWS(tracker.accept_delta(corrupt));
	TESTEQUALS(tracker.get_revision(), uint64_t{9});
	TESTEQUALS(tracker.get_tower_kills(101).value(), uint64_t{4});

	// No score update means the previously accepted total is retained.
	auto heartbeat = make_world_delta(recovered, no_change);
	TESTEQUALS(heartbeat.local_kills_changed, false);
	TESTEQUALS(tracker.accept_delta(heartbeat) == DeltaDisposition::APPLY, true);
	// Recovery remains visible even if a delta arrives before the next render frame.
	TESTEQUALS(tracker.get_presentation_state()->snapshot_generation, initial_generation + 1);
	TESTEQUALS(tracker.get_presentation_state()->world.world_revision, uint64_t{10});
	tracker.accept_snapshot(no_change);
	TESTEQUALS(tracker.get_presentation_state()->snapshot_generation, initial_generation + 2);
	TESTEQUALS(tracker.get_local_kills().value(), uint64_t{7});

	auto bad_snapshot = recovered;
	bad_snapshot.base_revision = 1;
	TESTTHROWS(tracker.accept_snapshot(bad_snapshot));

	// Host loss invalidates both the total and individual tower scores.
	tracker.host_disconnected();
	TESTEQUALS(tracker.is_host_connected(), false);
	TESTEQUALS(tracker.has_state(), false);
	TESTEQUALS(tracker.is_awaiting_snapshot(), false);
	TESTEQUALS(tracker.get_local_kills().has_value(), false);
	TESTEQUALS(tracker.get_tower_kills(101).has_value(), false);
	TESTEQUALS(tracker.get_world_state().has_value(), false);
	TESTEQUALS(tracker.get_presentation_state().has_value(), false);
	TESTEQUALS(tracker.get_revision(), uint64_t{10});
	TESTTHROWS(tracker.accept_snapshot(recovered));
	TESTTHROWS(tracker.accept_delta(heartbeat));
	TESTTHROWS(tracker.make_snapshot_request());

	// Recipients receive only their own private tower scores.
	ReplicationTracker other_player{3};
	auto other_snapshot = recovered;
	other_snapshot.local_player_id = 3;
	other_snapshot.local_kill_count = 12;
	other_snapshot.entities = {{
		200, 3, "fvd.data.entities.Tower", {2, 2, 0}, 100, 100, 12}};
	other_player.accept_snapshot(other_snapshot);
	TESTEQUALS(other_player.get_local_kills().value(), uint64_t{12});
	TESTEQUALS(other_player.get_tower_kills(200).value(), uint64_t{12});
	TESTEQUALS(other_player.get_tower_kills(100).has_value(), false);
}

} // namespace openage::network::tests
