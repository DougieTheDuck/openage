// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_mailbox.h"

#include <utility>

#include "error/error.h"

namespace openage::network {

CommandMailbox::CommandMailbox(size_t max_pending) :
	commands{max_pending} {}

bool CommandMailbox::try_push(AuthenticatedCommand command) {
	if (command.identity.session_id == 0 or command.identity.player_id == 0) {
		throw Error{MSG(err) << "FVD command mailbox requires an accepted session identity"};
	}

	return this->commands.try_push(std::move(command));
}

std::vector<AuthenticatedCommand> CommandMailbox::drain(size_t maximum_count) {
	return this->commands.drain(maximum_count);
}

size_t CommandMailbox::size() const {
	return this->commands.size();
}

size_t CommandMailbox::capacity() const {
	return this->commands.capacity();
}

} // namespace openage::network
