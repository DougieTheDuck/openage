// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_mailbox.h"

#include <atomic>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "testing/testing.h"

namespace openage::network::tests {

void command_mailbox() {
	TESTTHROWS(CommandMailbox{0});
	CommandMailbox mailbox{2};
	TESTEQUALS(mailbox.capacity(), size_t{2});
	TESTEQUALS(mailbox.size(), size_t{0});
	TESTTHROWS(mailbox.try_push({{}, {}}));

	AuthenticatedCommand first{{10, 2}, {1, CommandKind::MOVE, {100}, {1, 2, 0}}};
	AuthenticatedCommand second{{11, 3}, {2, CommandKind::BUILD_TOWER, {200}, {3, 4, 0}}};
	AuthenticatedCommand third{{12, 4}, {3, CommandKind::MOVE, {300}, {5, 6, 0}}};
	mailbox.try_push(first) or TESTFAIL;
	mailbox.try_push(second) or TESTFAIL;
	mailbox.try_push(third) and TESTFAIL;
	TESTEQUALS(mailbox.size(), size_t{2});

	auto drained = mailbox.drain(1);
	TESTEQUALS(drained.size(), size_t{1});
	TESTEQUALS(drained.front().identity.session_id, uint64_t{10});
	TESTEQUALS(drained.front().intent.command_id, uint64_t{1});
	mailbox.try_push(third) or TESTFAIL;
	drained = mailbox.drain(10);
	TESTEQUALS(drained.size(), size_t{2});
	TESTEQUALS(drained[0].intent.command_id, uint64_t{2});
	TESTEQUALS(drained[1].intent.command_id, uint64_t{3});

	constexpr size_t producer_count = 4;
	constexpr size_t commands_per_producer = 100;
	CommandMailbox concurrent_mailbox{producer_count * commands_per_producer};
	std::atomic<bool> producer_failed{};
	std::vector<std::jthread> producers{};
	for (size_t producer = 0; producer < producer_count; ++producer) {
		producers.emplace_back([&, producer]() {
			for (size_t command = 0; command < commands_per_producer; ++command) {
				AuthenticatedCommand queued{};
				queued.identity = {
					static_cast<uint64_t>(producer + 1),
					static_cast<gamestate::player_id_t>(producer + 1)};
				queued.intent.command_id = static_cast<uint64_t>(
					producer * commands_per_producer + command + 1);
				queued.intent.kind = CommandKind::MOVE;
				if (not concurrent_mailbox.try_push(std::move(queued))) {
					producer_failed.store(true);
					return;
				}
			}
		});
	}
	for (auto &producer : producers) {
		producer.join();
	}
	not producer_failed.load() or TESTFAIL;

	auto concurrent_commands = concurrent_mailbox.drain(
		producer_count * commands_per_producer);
	TESTEQUALS(concurrent_commands.size(),
	           producer_count * commands_per_producer);
	std::unordered_set<uint64_t> command_ids{};
	for (const auto &command : concurrent_commands) {
		command_ids.insert(command.intent.command_id);
	}
	TESTEQUALS(command_ids.size(), concurrent_commands.size());
}

} // namespace openage::network::tests
