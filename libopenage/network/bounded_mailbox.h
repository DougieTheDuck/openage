// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#include "error/error.h"

namespace openage::network {

/** Bounded, nonblocking FIFO mailbox shared across subsystem threads. */
template <typename T>
class BoundedMailbox {
public:
	/** Create a mailbox with room for at most max_pending values. */
	explicit BoundedMailbox(size_t max_pending) :
		max_pending{max_pending} {
		if (max_pending == 0) {
			throw Error{MSG(err) << "FVD mailbox capacity must be nonzero"};
		}
	}

	/** Attempt to enqueue one value without blocking. */
	bool try_push(T value) {
		std::scoped_lock lock{this->mutex};
		if (this->values.size() >= this->max_pending) {
			return false;
		}
		this->values.push_back(std::move(value));
		return true;
	}

	/** Remove up to maximum_count values in FIFO arrival order. */
	std::vector<T> drain(size_t maximum_count) {
		std::scoped_lock lock{this->mutex};
		auto count = std::min(maximum_count, this->values.size());
		std::vector<T> result{};
		result.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			result.push_back(std::move(this->values.front()));
			this->values.pop_front();
		}
		return result;
	}

	/** Number of values waiting for the consumer. */
	size_t size() const {
		std::scoped_lock lock{this->mutex};
		return this->values.size();
	}

	/** Maximum number of pending values. */
	size_t capacity() const {
		return this->max_pending;
	}

private:
	const size_t max_pending;
	mutable std::mutex mutex;
	std::deque<T> values;
};

} // namespace openage::network
