// Copyright 2026 the openage authors. See copying.md for legal info.

#include "world_delta.h"

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
		time::time_t::from_int(40), 90}};
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
	TESTEQUALS(applied.arrows.size(), size_t{1});
	TESTEQUALS(applied.arrows.front().arrow_id, uint64_t{56});
	// Arrow 55 expired at t=20.2 without a removal ID being transmitted.

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
}

} // namespace openage::network::tests
