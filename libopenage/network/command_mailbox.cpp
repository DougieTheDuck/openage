// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_mailbox.h"

#include <algorithm>
#include <utility>

#include "error/error.h"

namespace openage::network {

CommandMailbox::CommandMailbox(size_t max_pending) :
	max_pending{max_pending} {
	if (max_pending == 0) {
		throw Error{MSG(err) << "FVD command mailbox capacity must be nonzero"};
	}
}

bool CommandMailbox::try_push(AuthenticatedCommand command) {
	if (command.identity.session_id == 0 or command.identity.player_id == 0) {
		throw Error{MSG(err) << "FVD command mailbox requires an accepted session identity"};
	}

	std::scoped_lock lock{this->mutex};
	if (this->commands.size() >= this->max_pending) {
		return false;
	}
	this->commands.push_back(std::move(command));
	return true;
}

std::vector<AuthenticatedCommand> CommandMailbox::drain(size_t maximum_count) {
	std::scoped_lock lock{this->mutex};
	auto count = std::min(maximum_count, this->commands.size());
	std::vector<AuthenticatedCommand> result{};
	result.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		result.push_back(std::move(this->commands.front()));
		this->commands.pop_front();
	}
	return result;
}

size_t CommandMailbox::size() const {
	std::scoped_lock lock{this->mutex};
	return this->commands.size();
}

size_t CommandMailbox::capacity() const {
	return this->max_pending;
}

} // namespace openage::network
