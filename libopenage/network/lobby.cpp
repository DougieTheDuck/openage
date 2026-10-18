// Copyright 2026 the openage authors. See copying.md for legal info.

#include "lobby.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <random>
#include <unordered_set>
#include <utility>

#include "error/error.h"

namespace openage::network {

namespace {

void valid_name(const std::string &name) {
	if (name.empty() or name.size() > MAX_PARTICIPANT_NAME_SIZE
	    or std::all_of(name.begin(), name.end(), [](unsigned char ch) {
		    return ch == ' ' or ch == '\t';
	    })) {
		throw Error{MSG(err) << "FVD lobby participant name is empty or too long"};
	}
	for (const unsigned char ch : name) {
		if (ch < 0x20 or ch == 0x7f) {
			throw Error{MSG(err) << "FVD lobby participant name contains a control character"};
		}
	}
}

uint64_t draw_ticket(const HostLobby::draw_t &draw, uint64_t bound) {
	auto result = draw(bound);
	if (result >= bound) {
		throw Error{MSG(err) << "FVD lobby random draw is outside its exclusive bound"};
	}
	return result;
}

} // namespace

void validate_participant_snapshot(const ParticipantSnapshot &snapshot) {
	if (snapshot.revision == 0 or snapshot.host_participant_id == 0
	    or snapshot.participants.empty()
	    or snapshot.participants.size() > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "Invalid FVD lobby snapshot header or participant count"};
	}
	std::unordered_set<uint64_t> ids;
	size_t hosts = 0;
	for (const auto &participant : snapshot.participants) {
		valid_name(participant.name);
		if (participant.participant_id == 0
		    or not ids.insert(participant.participant_id).second) {
			throw Error{MSG(err) << "FVD lobby has an invalid or duplicate participant ID"};
		}
		if (participant.is_host) {
			++hosts;
			if (participant.participant_id != snapshot.host_participant_id) {
				throw Error{MSG(err) << "FVD lobby has a mismatched host"};
			}
		}
	}
	if (hosts != 1) {
		throw Error{MSG(err) << "FVD lobby requires exactly one host"};
	}
}

void validate_start_match(const StartMatch &start) {
	auto count = start.assignments.size();
	if (start.lobby_revision == 0 or start.host_participant_id == 0
	    or count < 2 or count > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "FVD match must start with 2-8 participants"};
	}
	std::unordered_set<uint64_t> participants;
	std::unordered_set<gamestate::player_id_t> players;
	size_t hunters = 0;
	bool contains_host = false;
	for (const auto &assignment : start.assignments) {
		if (assignment.participant_id == 0
		    or not participants.insert(assignment.participant_id).second
		    or assignment.player_id == 0
		    or assignment.player_id > count
		    or not players.insert(assignment.player_id).second) {
			throw Error{MSG(err) << "FVD match has duplicate, missing or invalid assignments"};
		}
		contains_host = contains_host or assignment.participant_id == start.host_participant_id;
		if (assignment.role == PlayerRole::HUNTER) {
			++hunters;
			if (assignment.player_id != 1) {
				throw Error{MSG(err) << "FVD Hunter must occupy player slot 1"};
			}
		}
		else if (assignment.role != PlayerRole::BUILDER or assignment.player_id == 1) {
			throw Error{MSG(err) << "FVD Builder must occupy a slot from 2 to N"};
		}
	}
	if (hunters != 1 or not contains_host) {
		throw Error{MSG(err) << "FVD match must have one Hunter and include its host"};
	}
}

void validate_start_match_against_roster(const ParticipantSnapshot &roster,
                                         const StartMatch &start) {
	validate_participant_snapshot(roster);
	validate_start_match(start);
	if (start.host_participant_id != roster.host_participant_id
	    or start.lobby_revision != roster.revision + 1
	    or start.assignments.size() != roster.participants.size()) {
		throw Error{MSG(err) << "FVD match assignments do not follow the latest lobby roster"};
	}
	std::unordered_set<uint64_t> roster_ids;
	for (const auto &participant : roster.participants) {
		roster_ids.insert(participant.participant_id);
	}
	for (const auto &assignment : start.assignments) {
		if (not roster_ids.contains(assignment.participant_id)) {
			throw Error{MSG(err) << "FVD match contains a participant not admitted to the lobby"};
		}
	}
}

gamestate::player_id_t player_for_participant(const StartMatch &start,
                                               uint64_t participant_id) {
	validate_start_match(start);
	for (const auto &assignment : start.assignments) {
		if (assignment.participant_id == participant_id) {
			return assignment.player_id;
		}
	}
	throw Error{MSG(err) << "FVD match has no assignment for participant " << participant_id};
}

std::string describe_match_roles(const ParticipantSnapshot &roster,
                                 const StartMatch &start) {
	validate_start_match_against_roster(roster, start);
	std::string result;
	for (const auto &participant : roster.participants) {
		for (const auto &assignment : start.assignments) {
			if (assignment.participant_id != participant.participant_id) {
				continue;
			}
			if (not result.empty()) {
				result += "\n";
			}
			result += participant.name;
			result += " — ";
			result += assignment.role == PlayerRole::HUNTER ? "HUNTER" : "BUILDER";
			result += " (Player " + std::to_string(assignment.player_id) + ")";
			break;
		}
	}
	return result;
}

uint8_t role_reveal_seconds_remaining(std::chrono::milliseconds elapsed) {
	constexpr int64_t reveal_ms = 5000;
	const auto ms = elapsed.count();
	if (ms >= reveal_ms) {
		return 0;
	}
	if (ms <= 0) {
		return 5;
	}
	return static_cast<uint8_t>((reveal_ms - ms + 999) / 1000);
}

HostLobby::HostLobby(uint64_t host_id, std::string host_name) {
	this->current = {1, host_id, {{host_id, std::move(host_name), true}}};
	validate_participant_snapshot(this->current);
}

void HostLobby::join(uint64_t participant_id, std::string name) {
	if (this->match_started) {
		throw Error{MSG(err) << "Cannot join an FVD lobby after the match starts"};
	}
	if (this->current.participants.size() == MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "FVD lobby is full (8 players)"};
	}
	valid_name(name);
	if (participant_id == 0 or std::any_of(
	    this->current.participants.begin(), this->current.participants.end(),
	    [participant_id](const auto &p) { return p.participant_id == participant_id; })) {
		throw Error{MSG(err) << "FVD lobby participant ID is invalid or already connected"};
	}
	if (this->current.revision == std::numeric_limits<uint64_t>::max()) {
		throw Error{MSG(err) << "FVD lobby revision exhausted"};
	}
	this->current.participants.push_back({participant_id, std::move(name), false});
	++this->current.revision;
}

void HostLobby::leave(uint64_t participant_id) {
	if (this->match_started) {
		throw Error{MSG(err) << "Match participants cannot leave via pre-start lobby"};
	}
	if (participant_id == this->current.host_participant_id) {
		throw Error{MSG(err) << "Closing the host lobby requires closing the whole session"};
	}
	auto pos = std::find_if(this->current.participants.begin(),
	                        this->current.participants.end(),
	                        [participant_id](const auto &p) {
		                        return p.participant_id == participant_id;
	                        });
	if (pos == this->current.participants.end()) {
		throw Error{MSG(err) << "FVD lobby departure names an unknown participant"};
	}
	if (this->current.revision == std::numeric_limits<uint64_t>::max()) {
		throw Error{MSG(err) << "FVD lobby revision exhausted"};
	}
	this->current.participants.erase(pos);
	++this->current.revision;
}

ParticipantSnapshot HostLobby::snapshot() const {
	return this->current;
}

StartMatch HostLobby::start(const draw_t &draw) {
	if (this->match_started) {
		throw Error{MSG(err) << "FVD lobby has already started"};
	}
	if (not draw) {
		throw Error{MSG(err) << "FVD lobby needs a random draw source"};
	}
	const auto count = this->current.participants.size();
	if (count < 2) {
		throw Error{MSG(err) << "FVD match needs at least two humans"};
	}
	if (this->current.revision == std::numeric_limits<uint64_t>::max()) {
		throw Error{MSG(err) << "FVD lobby revision exhausted"};
	}

	// The host has one ticket, each admitted guest has two. Drawing uniformly
	// over 2*N-1 tickets implements the specified 0.5:1 relative weighting.
	const auto ticket = draw_ticket(draw, count * 2 - 1);
	const size_t hunter_index = ticket == 0 ? 0 : 1 + (ticket - 1) / 2;

	std::vector<gamestate::player_id_t> builders{};
	for (size_t id = 2; id <= count; ++id) {
		builders.push_back(id);
	}
	// Fisher-Yates: randomly allocate unique Builder slots independent of
	// the Hunter draw, with rejection of biased/out-of-range injected draws.
	for (size_t n = builders.size(); n > 1; --n) {
		const auto i = draw_ticket(draw, n);
		std::swap(builders[n - 1], builders[i]);
	}

	StartMatch result{};
	result.lobby_revision = this->current.revision + 1;
	result.host_participant_id = this->current.host_participant_id;
	result.assignments.reserve(count);
	size_t builder_index = 0;
	for (size_t i = 0; i < count; ++i) {
		const auto &participant = this->current.participants[i];
		if (i == hunter_index) {
			result.assignments.push_back({
				participant.participant_id, 1, PlayerRole::HUNTER});
		}
		else {
			result.assignments.push_back({
				participant.participant_id, builders[builder_index++], PlayerRole::BUILDER});
		}
	}
	validate_start_match(result);
	this->current.revision = result.lobby_revision;
	this->match_started = true;
	return result;
}

StartMatch HostLobby::start() {
	std::random_device source;
	std::seed_seq seed{
		source(), source(), source(), source(),
		source(), source(), source(), source(),
	};
	std::mt19937_64 random{seed};
	return this->start([&random](uint64_t bound) {
		return std::uniform_int_distribution<uint64_t>{0, bound - 1}(random);
	});
}

bool HostLobby::started() const {
	return this->match_started;
}

size_t HostLobby::size() const {
	return this->current.participants.size();
}

} // namespace openage::network
