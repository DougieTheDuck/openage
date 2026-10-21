// Copyright 2026 the openage authors. See copying.md for legal info.

#include "world_delta.h"

#include <utility>

#include "testing/testing.h"

namespace openage::network::tests {

void world_delta() {
	WorldState initial{};
	initial.map_size = 128;
	initial.local_player_id = 2;
	initial.world_revision = 10;
	initial.simulation_time = time::time_t::from_int(20);
	initial.local_kill_count = 4;
	initial.players = {{1, PlayerRole::HUNTER}, {2, PlayerRole::BUILDER},
	                   {3, PlayerRole::BUILDER}};
	initial.entities = {
		{10, 1, "fvd.data.entities.Horse", {60, 60, 0}, 25, 25},
		{11, 2, "fvd.data.entities.Tower", {65, 65, 0}, 100, 100, 4},
		{12, 3, "fvd.data.entities.Tower", {70, 70, 0}, 100, 100},
		{13, 2, "fvd.data.entities.Builder", {66, 66, 0}, 25, 25},
	};
	initial.reservations = {{{66, 67}, 13}};
	initial.constructions = {{
		7, 2, 13, {66, 67}, ConstructionPhase::PLANNED,
		time::time_t::from_int(40), time::time_t::from_int(40), 90}};
	initial.attack_orders = {{10, {65, 65, 0}, 11, false, true}};
	initial.arrows = {{
		55, 2, 10, {65, 65, 1}, {60, 60, 0},
		time::time_t::from_int(20), time::time_t::from_double(20.2)}};
	// The guest can stop drawing at the prescribed time, without a packet.
	TESTEQUALS(arrow_visible_at(
		initial.arrows.front(), time::time_t::from_double(19.9)), false);
	TESTEQUALS(arrow_visible_at(
		initial.arrows.front(), time::time_t::from_double(20.1)), true);
	TESTEQUALS(arrow_visible_at(
		initial.arrows.front(), time::time_t::from_double(20.2)), false);

	// Reconstruct construction timing even when only the starting time changes.
	auto constructing = initial;
	constructing.world_revision = 11;
	constructing.simulation_time = time::time_t::from_int(25);
	constructing.constructions.front().phase = ConstructionPhase::UNDER_CONSTRUCTION;
	constructing.constructions.front().phase_start_time = time::time_t::from_int(20);
	auto corrected = constructing;
	corrected.world_revision = 12;
	corrected.simulation_time = time::time_t::from_int(26);
	corrected.constructions.front().phase_start_time = time::time_t::from_int(21);
	auto progress_delta = make_world_delta(constructing, corrected);
	TESTEQUALS(progress_delta.changed.constructions.size(), size_t{1});
	auto construction_recovered = apply_world_delta(constructing,
		decode_world_delta(encode_world_delta(progress_delta)));
	TESTEQUALS(construction_recovered.constructions.front().phase_start_time,
	           time::time_t::from_int(21));
	TESTEQUALS(construction_recovered.constructions.front().progress_percent(
	           time::time_t::from_int(30)), 47);

	WorldDeltaBaseline host{};
	TESTEQUALS(host.initialized(), false);
	TESTTHROWS(host.advance(initial));
	host.accept_snapshot(initial);
	TESTEQUALS(host.initialized(), true);

	// A time-only revision contains no repeated entities, arrows, scores,
	// players, construction, orders, or reservations.
	auto no_change = initial;
	no_change.world_revision = 11;
	no_change.simulation_time = time::time_t::from_double(20.1);
	auto heartbeat = host.advance(no_change);
	TESTEQUALS(heartbeat.local_kills_changed, false);
	TESTEQUALS(heartbeat.changed.entities.empty(), true);
	TESTEQUALS(heartbeat.changed.players.empty(), true);
	TESTEQUALS(heartbeat.changed.arrows.empty(), true);
	TESTEQUALS(heartbeat.changed.local_kill_count, uint64_t{0});
	auto encoded_heartbeat = encode_world_delta(heartbeat);
	auto baseline_bytes = encode_world_state(no_change);
	TESTEQUALS(encoded_heartbeat.size() < baseline_bytes.size(), true);

	auto rebuilt = apply_world_delta(initial, decode_world_delta(encoded_heartbeat));
	TESTEQUALS(rebuilt.world_revision, uint64_t{11});
	TESTEQUALS(rebuilt.entities.size(), size_t{4});
	TESTEQUALS(rebuilt.arrows.size(), size_t{1});
	TESTEQUALS(rebuilt.local_kill_count, uint64_t{4});

	// Replace one entity, insert another, delete the old horse; persistent
	// objects gain tombstones but arrows expire from their impact time.
	auto changed = no_change;
	changed.world_revision = 12;
	changed.simulation_time = time::time_t::from_double(20.2);
	changed.local_kill_count = 5;
	changed.players[2].connected = false;
	changed.entities.erase(changed.entities.begin());
	changed.entities[0].tower_kills = 5;
	changed.entities.push_back({14, 1, "fvd.data.entities.Horse", {61, 61, 0}, 25, 25});
	changed.reservations.clear();
	changed.constructions.clear();
	changed.attack_orders.clear();
	changed.arrows.clear();
	changed.arrows.push_back({
		56, 2, 14, {65, 65, 1}, {61, 61, 0},
		time::time_t::from_double(20.2), time::time_t::from_double(20.4)});
	auto delta = host.advance(changed);
	TESTEQUALS(delta.local_kills_changed, true);
	TESTEQUALS(delta.changed.local_kill_count, uint64_t{5});
	TESTEQUALS(delta.changed.players.size(), size_t{1});
	TESTEQUALS(delta.changed.entities.size(), size_t{2});
	TESTEQUALS(delta.removed_entities.size(), size_t{1});
	TESTEQUALS(delta.removed_entities.front(), gamestate::entity_id_t{10});
	TESTEQUALS(delta.removed_reservations.size(), size_t{1});
	TESTEQUALS(delta.removed_constructions.size(), size_t{1});
	TESTEQUALS(delta.removed_attack_orders.size(), size_t{1});
	TESTEQUALS(delta.changed.arrows.size(), size_t{1});
	auto encoded = encode_world_delta(delta);
	TESTEQUALS(encoded.size() < encode_world_state(changed).size(), true);
	auto decoded = decode_world_delta(encoded);
	auto applied = apply_world_delta(rebuilt, decoded);
	TESTEQUALS(applied.local_kill_count, uint64_t{5});
	TESTEQUALS(applied.players.at(2).connected, false);
	TESTEQUALS(applied.entities.size(), size_t{4});
	TESTEQUALS(applied.entities.at(0).entity_id, gamestate::entity_id_t{11});
	TESTEQUALS(applied.entities.at(0).tower_kills.value(), uint64_t{5});
	TESTEQUALS(applied.entities.at(3).entity_id, gamestate::entity_id_t{14});
	TESTEQUALS(applied.reservations.empty(), true);
	TESTEQUALS(applied.constructions.empty(), true);
	TESTEQUALS(applied.attack_orders.empty(), true);
	// Arrow 55 already hit at t=20.2, but its launch survives briefly
	// so a slow guest can render its missed flight exactly once.
	TESTEQUALS(applied.arrows.size(), size_t{2});
	TESTEQUALS(applied.arrows.front().arrow_id, uint64_t{55});
	TESTEQUALS(applied.arrows.back().arrow_id, uint64_t{56});

	// A 0.2-second projectile can launch and hit between successive
	// recipient captures. Keep its unique launch event for the guest's
	// short visual replay window, without ever reapplying combat damage.
	WorldState before_shot{};
	before_shot.world_revision = 1;
	before_shot.simulation_time = time::time_t::from_int(10);
	before_shot.map_size = 128;
	before_shot.local_player_id = 2;
	before_shot.players = {{1, PlayerRole::HUNTER}, {2, PlayerRole::BUILDER}};
	WorldDeltaBaseline event_host{};
	event_host.accept_snapshot(before_shot);
	auto after_shot = before_shot;
	after_shot.world_revision = 2;
	after_shot.simulation_time = time::time_t::from_double(10.7);
	after_shot.arrows = {{
		100, 2, 999, {60, 60, 1}, {61, 61, 0},
		time::time_t::from_double(10.1),
		time::time_t::from_double(10.3)}};
	auto late_event = event_host.advance(after_shot);
	TESTEQUALS(late_event.changed.arrows.size(), size_t{1});
	TESTEQUALS(arrow_visible_at(after_shot.arrows.front(),
	                           after_shot.simulation_time), false);
	auto event_guest = apply_world_delta(
		before_shot, decode_world_delta(encode_world_delta(late_event)));
	TESTEQUALS(event_guest.arrows.size(), size_t{1});
	TESTEQUALS(event_guest.arrows.front().arrow_id, uint64_t{100});
	auto next_shot_frame = after_shot;
	next_shot_frame.world_revision = 3;
	next_shot_frame.simulation_time = time::time_t::from_int(11);
	auto deduplicated = event_host.advance(next_shot_frame);
	TESTEQUALS(deduplicated.changed.arrows.empty(), true);
	event_guest = apply_world_delta(event_guest, deduplicated);
	TESTEQUALS(event_guest.arrows.size(), size_t{1});
	auto after_history = next_shot_frame;
	after_history.world_revision = 4;
	after_history.simulation_time = time::time_t::from_int(14);
	after_history.arrows.clear();
	auto expired = event_host.advance(after_history);
	TESTEQUALS(expired.changed.arrows.empty(), true);
	event_guest = apply_world_delta(event_guest, expired);
	TESTEQUALS(event_guest.arrows.empty(), true);
	auto invalid_old_event = late_event;
	invalid_old_event.changed.simulation_time = time::time_t::from_int(14);
	TESTTHROWS(apply_world_delta(before_shot, invalid_old_event));

	// Corruption, duplicate removals, wrong players and gaps cannot
	// change a client's last accepted state.
	auto tampered = encoded;
	tampered.pop_back();
	TESTTHROWS(decode_world_delta(tampered));
	auto trailing = encoded;
	trailing.push_back(0);
	TESTTHROWS(decode_world_delta(trailing));
	auto bad_flags = encoded;
	bad_flags[1] |= 0x80;
	TESTTHROWS(decode_world_delta(bad_flags));
	auto dup = decoded;
	dup.removed_entities.push_back(10);
	TESTTHROWS(apply_world_delta(rebuilt, dup));
	auto unknown = decoded;
	unknown.removed_entities = {999};
	TESTTHROWS(apply_world_delta(rebuilt, unknown));
	auto wrong_player = decoded;
	wrong_player.changed.local_player_id = 3;
	TESTTHROWS(apply_world_delta(rebuilt, wrong_player));
	auto gap = decoded;
	gap.changed.base_revision = 9;
	TESTTHROWS(apply_world_delta(rebuilt, gap));
	auto leak = decoded;
	leak.changed.entities.push_back({
		15, 3, "fvd.data.entities.Tower", {70, 70, 0}, 100, 100, 11});
	TESTTHROWS(encode_world_delta(leak));
	auto malformed_count = encoded;
	malformed_count[11] = 0xff; // removal count too large
	TESTTHROWS(decode_world_delta(malformed_count));

	// A complete recovery snapshot restores the missing data and becomes
	// the next delta baseline; a client cannot apply out-of-order patches.
	auto recovered = changed;
	recovered.world_revision = 30;
	host.accept_snapshot(recovered);
	auto after_recovery = recovered;
	after_recovery.world_revision = 31;
	auto next = host.advance(after_recovery);
	TESTEQUALS(next.changed.base_revision, uint64_t{30});
	TESTEQUALS(apply_world_delta(recovered, next).world_revision, uint64_t{31});

	// A retasked movement curve must be delivered even if the sampled
	// position has not yet changed. Later clock-only deltas retire played
	// keys locally, without a keyframe tombstone or redundant entity patch.
	WorldState curve_start{};
	curve_start.world_revision = 1;
	curve_start.map_size = 128;
	curve_start.local_player_id = 2;
	curve_start.simulation_time = time::time_t::from_int(1);
	curve_start.players = {{1, PlayerRole::HUNTER}, {2, PlayerRole::BUILDER}};
	curve_start.entities = {
		{77, 2, "fvd.data.entities.Builder", {2, 2, 0}, 25, 25}};
	curve_start.entities.front().motion_keys = {
		{time::time_t::from_int(2), {3, 2, 0}},
		{time::time_t::from_int(3), {4, 2, 0}},
	};
	WorldDeltaBaseline curves{};
	curves.accept_snapshot(curve_start);
	auto retask = curve_start;
	retask.world_revision = 2;
	retask.simulation_time = time::time_t::from_double(1.1);
	retask.entities.front().motion_keys = {
		{time::time_t::from_int(2), {2, 3, 0}},
		{time::time_t::from_int(3), {2, 4, 0}},
	};
	auto reroute_delta = curves.advance(retask);
	TESTEQUALS(reroute_delta.changed.entities.size(), size_t{1});
	auto guest_retask = apply_world_delta(
		curve_start, decode_world_delta(encode_world_delta(reroute_delta)));
	TESTEQUALS(guest_retask.entities.front().motion_keys[0].position,
	           (coord::phys3{2, 3, 0}));
	auto later = retask;
	later.world_revision = 3;
	later.simulation_time = time::time_t::from_double(2.25);
	// A real capture samples the current position and sends only FUTURE
	// keys; it must not require an entity patch when the path is unchanged.
	later.entities.front().position = {2, 3.25, 0};
	later.entities.front().motion_keys.erase(later.entities.front().motion_keys.begin());
	auto clock_only = curves.advance(later);
	TESTEQUALS(clock_only.changed.entities.empty(), true);
	auto presented = apply_world_delta(guest_retask, clock_only);
	TESTEQUALS(presented.entities.front().motion_keys.size(), size_t{1});
	TESTEQUALS(presented.entities.front().motion_keys.front().time,
	           time::time_t::from_int(3));
	TESTEQUALS(presented.entities.front().position, (coord::phys3{2, 3.25, 0}));

	// Scheduled facing changes cross time-only deltas without redundant
	// entity traffic. An unexpected turn must transmit a correction.
	auto facing_start = curve_start;
	facing_start.entities.front().angle = coord::phys_angle_t::from_int(315);
	facing_start.entities.front().facing_keys = {
		{time::time_t::from_int(2), coord::phys_angle_t::from_int(270)},
		{time::time_t::from_int(3), coord::phys_angle_t::from_int(90)},
	};
	WorldDeltaBaseline facing_host{};
	facing_host.accept_snapshot(facing_start);
	auto facing_later = facing_start;
	facing_later.world_revision = 2;
	facing_later.simulation_time = time::time_t::from_double(2.25);
	facing_later.entities.front().position = {3.25, 2, 0};
	facing_later.entities.front().motion_keys.erase(
		facing_later.entities.front().motion_keys.begin());
	facing_later.entities.front().angle = coord::phys_angle_t::from_int(270);
	facing_later.entities.front().facing_keys.erase(
		facing_later.entities.front().facing_keys.begin());
	auto facing_heartbeat = facing_host.advance(facing_later);
	TESTEQUALS(facing_heartbeat.changed.entities.empty(), true);
	auto facing_guest = apply_world_delta(
		facing_start, decode_world_delta(encode_world_delta(facing_heartbeat)));
	TESTEQUALS(facing_guest.entities.front().angle,
	           coord::phys_angle_t::from_int(270));
	TESTEQUALS(facing_guest.entities.front().facing_keys.size(), size_t{1});
	TESTEQUALS(facing_guest.entities.front().facing_keys.front().angle,
	           coord::phys_angle_t::from_int(90));
	auto facing_retarget = facing_later;
	facing_retarget.world_revision = 3;
	facing_retarget.simulation_time = time::time_t::from_double(2.5);
	facing_retarget.entities.front().position = {3.5, 2, 0};
	facing_retarget.entities.front().angle = coord::phys_angle_t::from_int(180);
	facing_retarget.entities.front().facing_keys.clear();
	auto facing_patch = facing_host.advance(facing_retarget);
	TESTEQUALS(facing_patch.changed.entities.size(), size_t{1});
	facing_guest = apply_world_delta(facing_guest,
		decode_world_delta(encode_world_delta(facing_patch)));
	TESTEQUALS(facing_guest.entities.front().angle,
	           coord::phys_angle_t::from_int(180));
	TESTEQUALS(facing_guest.entities.front().facing_keys.empty(), true);

	// A new idle/walk action changes the sprite but identical later samples
	// keep the same host-authored animation phase without entity traffic.
	auto animation_start = curve_start;
	WorldDeltaBaseline animation_host{};
	animation_host.accept_snapshot(animation_start);
	auto walking = animation_start;
	walking.world_revision = 2;
	walking.simulation_time = time::time_t::from_double(1.5);
	walking.entities.front().position = {2.5, 2, 0};
	walking.entities.front().animation_action = gamestate::AnimationAction::MOVE;
	walking.entities.front().animation_started_at = time::time_t::from_double(1.25);
	auto walking_delta = animation_host.advance(walking);
	TESTEQUALS(walking_delta.changed.entities.size(), size_t{1});
	auto walking_guest = apply_world_delta(
		animation_start, decode_world_delta(encode_world_delta(walking_delta)));
	TESTEQUALS(walking_guest.entities.front().animation_action,
	           gamestate::AnimationAction::MOVE);
	TESTEQUALS(walking_guest.entities.front().animation_started_at,
	           time::time_t::from_double(1.25));
	auto same_walk = walking;
	same_walk.world_revision = 3;
	same_walk.simulation_time = time::time_t::from_double(1.75);
	same_walk.entities.front().position = {2.75, 2, 0};
	auto no_animation_patch = animation_host.advance(same_walk);
	TESTEQUALS(no_animation_patch.changed.entities.empty(), true);
	walking_guest = apply_world_delta(walking_guest, no_animation_patch);
	TESTEQUALS(walking_guest.entities.front().animation_started_at,
	           time::time_t::from_double(1.25));
	auto idle_again = same_walk;
	idle_again.world_revision = 4;
	idle_again.simulation_time = time::time_t::from_int(2);
	idle_again.entities.front().position = {3, 2, 0};
	idle_again.entities.front().animation_action = gamestate::AnimationAction::IDLE;
	idle_again.entities.front().animation_started_at = time::time_t::from_int(2);
	auto idle_delta = animation_host.advance(idle_again);
	TESTEQUALS(idle_delta.changed.entities.size(), size_t{1});
	walking_guest = apply_world_delta(walking_guest, idle_delta);
	TESTEQUALS(walking_guest.entities.front().animation_action,
	           gamestate::AnimationAction::IDLE);
	TESTEQUALS(walking_guest.entities.front().animation_started_at,
	           time::time_t::from_int(2));

	// Even with hundreds of Hunter horses and 2/4/8 player rosters,
	// an unchanged scheduled route needs no repeated per-horse payload.
	// These are deterministic packet-size checks, not wall-clock benchmarks.
	for (size_t player_count : {size_t{2}, size_t{4}, size_t{8}}) {
		WorldState dense{};
		dense.map_size = 128;
		dense.world_revision = 1;
		dense.local_player_id = 2;
		for (size_t player = 1; player <= player_count; ++player) {
			dense.players.push_back({
				static_cast<gamestate::player_id_t>(player),
				player == 1 ? PlayerRole::HUNTER : PlayerRole::BUILDER});
		}
		for (size_t i = 0; i < 512; ++i) {
			EntityState horse{
				static_cast<gamestate::entity_id_t>(1000 + i),
				1, "fvd.data.entities.Horse",
				{static_cast<int64_t>(i), 0, 0}, 90, 90};
			horse.motion_keys.push_back({
				time::time_t::from_int(64),
				{static_cast<int64_t>(i + 64), 0, 0}});
			dense.entities.push_back(std::move(horse));
		}
		WorldDeltaBaseline dense_host{};
		dense_host.accept_snapshot(dense);
		auto dense_guest = dense;
		const auto full_payload_size = encode_world_state(dense).size();
		for (int64_t tick = 1; tick <= 24; ++tick) {
			auto current = dense;
			current.world_revision = static_cast<uint64_t>(tick + 1);
			current.simulation_time = time::time_t::from_int(tick);
			for (size_t i = 0; i < current.entities.size(); ++i) {
				current.entities[i].position = {
					static_cast<int64_t>(i) + tick, 0, 0};
			}
			auto delta = dense_host.advance(current);
			TESTEQUALS(delta.changed.entities.empty(), true);
			const auto encoded = encode_world_delta(delta);
			// An empty-diff packet must remain tiny despite 512 movers.
			TESTEQUALS(encoded.size() * 10 < full_payload_size, true);
			dense_guest = apply_world_delta(
				dense_guest, decode_world_delta(encoded));
			TESTEQUALS(dense_guest.entities.size(), size_t{512});
			TESTEQUALS(dense_guest.world_revision, current.world_revision);
			TESTEQUALS(dense_guest.entities.front().position,
			           current.entities.front().position);
			TESTEQUALS(dense_guest.entities.back().position,
			           current.entities.back().position);
		}
		// Any disagreement with the scheduled position is a correction,
		// not something the guest is permitted to infer or ignore.
		auto corrected = dense;
		corrected.world_revision = 26;
		corrected.simulation_time = time::time_t::from_int(25);
		for (size_t i = 0; i < corrected.entities.size(); ++i) {
			corrected.entities[i].position = {
				static_cast<int64_t>(i + 25), 0, 0};
		}
		corrected.entities.front().position = {30, 0, 0};
		auto correction = dense_host.advance(corrected);
		TESTEQUALS(correction.changed.entities.size(), size_t{1});
		TESTEQUALS(correction.changed.entities.front().entity_id,
		           gamestate::entity_id_t{1000});
		dense_guest = apply_world_delta(dense_guest, correction);
		TESTEQUALS(dense_guest.entities.front().position, (coord::phys3{30, 0, 0}));
	}
}

} // namespace openage::network::tests
