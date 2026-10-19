// Copyright 2026 the openage authors. See copying.md for legal info.

#include "rehost_ticket.h"

#include <algorithm>

#include "error/error.h"
#include "testing/testing.h"

namespace openage::network::tests {

void rehost_ticket() {
	HostLobby old{1, "Builder Host"};
	old.join(2, "Hunter Guest");
	old.join(3, "Other Builder");
	auto assignment = old.start([](uint64_t bound) {
		return bound == 5 ? uint64_t{1} : uint64_t{0};
	});
	TESTEQUALS(player_for_participant(assignment, 2), gamestate::player_id_t{1});
	const auto old_host_player = player_for_participant(assignment, 1);
	const auto other_builder = player_for_participant(assignment, 3);
	TESTEQUALS(old_host_player != 1, true);
	TESTEQUALS(other_builder != 1, true);
	TESTEQUALS(old_host_player != other_builder, true);
	TESTTHROWS(RehostTicketBook(assignment, 0));
	TESTTHROWS(RehostTicketBook(assignment, 99, 1));

	RehostTicketBook tickets{assignment, 8675309};
	TESTEQUALS(tickets.expected_guests(), size_t{2});
	TESTEQUALS(tickets.complete(), false);
	TESTTHROWS(tickets.offer_for(1));
	TESTTHROWS(tickets.offer_for(8));
	const auto offer_a = tickets.offer_for(old_host_player);
	const auto offer_b = tickets.offer_for(other_builder);
	TESTEQUALS(offer_a.original_lobby_revision, assignment.lobby_revision);
	TESTEQUALS(offer_a.authority_epoch, uint64_t{2});
	TESTEQUALS(offer_a.room_id, uint64_t{8675309});
	TESTEQUALS(offer_a.ticket == offer_b.ticket, false);

	const auto serialized = encode_rehost_offer(offer_a);
	TESTEQUALS(serialized.size(), size_t{64});
	const auto decoded = decode_rehost_offer(serialized);
	TESTEQUALS(decoded.original_player_id, old_host_player);
	TESTEQUALS(decoded.ticket == offer_a.ticket, true);
	auto proof_a = make_rehost_proof(decoded);
	TESTEQUALS(decode_rehost_proof(encode_rehost_proof(proof_a)).ticket
	           == offer_a.ticket, true);
	auto wrong_size = serialized;
	wrong_size.pop_back();
	TESTTHROWS(decode_rehost_offer(wrong_size));
	wrong_size.push_back(0);
	wrong_size.push_back(0);
	TESTTHROWS(decode_rehost_proof(wrong_size));
	auto zero_ticket = offer_a;
	zero_ticket.ticket.fill(0);
	TESTTHROWS(encode_rehost_offer(zero_ticket));
	auto impossible_player = proof_a;
	impossible_player.original_player_id = 9;
	TESTTHROWS(encode_rehost_proof(impossible_player));

	auto tampered = proof_a;
	tampered.ticket[8] ^= 1;
	TESTEQUALS(tickets.redeem(tampered, {4321, 2}), false);
	tampered = proof_a;
	tampered.room_id = 99;
	TESTEQUALS(tickets.redeem(tampered, {4321, 2}), false);
	tampered = proof_a;
	tampered.original_lobby_revision += 1;
	TESTEQUALS(tickets.redeem(tampered, {4321, 2}), false);
	tampered = proof_a;
	tampered.authority_epoch += 1;
	TESTEQUALS(tickets.redeem(tampered, {4321, 2}), false);
	TESTEQUALS(tickets.redeem(proof_a, {0, 2}), false);
	TESTEQUALS(tickets.redeem(proof_a, {4321, 1}), false);
	TESTEQUALS(tickets.redeem(proof_a, {4321, 2}), true);
	TESTEQUALS(tickets.has_redeemed(old_host_player), true);
	TESTEQUALS(tickets.redeem(proof_a, {4322, 3}), false);
	TESTEQUALS(tickets.redeem(make_rehost_proof(offer_b), {4321, 3}), false);
	TESTEQUALS(tickets.complete(), false);
	TESTEQUALS(tickets.redeem(make_rehost_proof(offer_b), {4322, 3}), true);
	TESTEQUALS(tickets.complete(), true);
	const auto &new_slots = tickets.new_participant_slots();
	TESTEQUALS(new_slots.size(), size_t{3});
	TESTEQUALS(new_slots.at(1), gamestate::player_id_t{1});
	TESTEQUALS(new_slots.at(2), old_host_player);
	TESTEQUALS(new_slots.at(3), other_builder);
	TESTEQUALS(tickets.redeem(make_rehost_proof(offer_b), {98765, 4}), false);
}

} // namespace openage::network::tests
