// Copyright 2017-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "event/eventhandler.h"
#include "event/eventqueue.h"
#include "time/time.h"


namespace openage::event {

// The demo wants to display internal details
namespace demo {
int curvepong();
}

class Event;
class EventEntity;
class State;


/**
 * The core class to manage event handler and targets.
 */
class EventLoop {
	// because the demo function displays internal info.
	friend int demo::curvepong();

public:
	/**
	 * Create a new event loop.
	 */
	EventLoop() = default;
	~EventLoop() = default;

	/**
	 * Register a new event handler.
	 *
	 * Created event can reference the event handler ID to invoke it on
	 * execution.
	 *
	 * @param eventhandler Event handler.
	 */
	void add_event_handler(const std::shared_ptr<EventHandler> eventhandler);

	/**
	 * Add a new event to the queue using a registered event handler.
	 *
	 * Blocks while the loop is executing events, so threads other than the
	 * one calling \p reach_time() should use \p submit_event() instead.
	 *
	 * @param eventhandler Event handler ID. The handler must already be registered on the loop.
	 * @param target Target entity. Can be \p nullptr.
	 * @param state Global state.
	 * @param reference_time Reference time to calculate the event execution time. The actual
	 *                       depends execution time on the type of event and may be changed
	 *                       by other events.
	 * @param params Event parameters map (default = {}). Passed to the event handler on event execution.
	 */
	std::shared_ptr<Event> create_event(const std::string eventhandler,
	                                    const std::shared_ptr<EventEntity> target,
	                                    const std::shared_ptr<State> state,
	                                    const time::time_t reference_time,
	                                    const EventHandler::param_map params = EventHandler::param_map({}));

	/**
	 * Add a new event to the queue using an arbritary event handler. If an event handler
	 * with the same ID is already registered, the registered event handler will be used
	 * instead.
	 *
	 * TODO: Why use this function when one can simply add the event handler and use the other
	 *      create_event function?
	 *
	 * @param eventhandler Event handler.
	 * @param target Target entity. Can be \p nullptr.
	 * @param state Global state.
	 * @param reference_time Reference time to calculate the event execution time. The actual
	 *                       depends execution time on the type of event and may be changed
	 *                       by other events.
	 * @param params Event parameters map (default = {}). Passed to the event handler on event execution.
	 */
	std::shared_ptr<Event> create_event(const std::shared_ptr<EventHandler> eventhandler,
	                                    const std::shared_ptr<EventEntity> target,
	                                    const std::shared_ptr<State> state,
	                                    const time::time_t reference_time,
	                                    const EventHandler::param_map params = EventHandler::param_map({}));

	/**
	 * Submit an event from outside the thread executing the loop, e.g. from input handling.
	 *
	 * The event is put into an inbox without waiting for the loop's event execution.
	 * The next call to \p reach_time() creates it like \p create_event() would,
	 * before any events are executed.
	 *
	 * @param eventhandler Event handler ID. The handler must be registered on the loop
	 *                     when the inbox is drained, else \p reach_time() throws.
	 * @param target Target entity. Can be \p nullptr.
	 * @param state Global state.
	 * @param reference_time Reference time to calculate the event execution time.
	 * @param params Event parameters map (default = {}). Passed to the event handler on event execution.
	 */
	void submit_event(const std::string eventhandler,
	                  const std::shared_ptr<EventEntity> target,
	                  const std::shared_ptr<State> state,
	                  const time::time_t reference_time,
	                  const EventHandler::param_map params = EventHandler::param_map({}));

	/**
	 * Create the events in the inbox, then execute events in the queue
	 * with execution time <= a given point in time.
	 *
	 * @param time_until Maximum time until which events are executed.
	 * @param state Global state.
	 */
	void reach_time(const time::time_t &time_until,
	                const std::shared_ptr<State> &state);

	/**
	 * Initiate a reevaluation of a given event at a given time.
	 *
	 * This usually happens because this event depended on an event entity
	 * that got changed at this time.
	 *
	 * This inserts the event into the changes queue
	 * so it will be evaluated in the next loop iteration.
	 *
	 * @param event Event to reevaluate.
	 * @param changes_at Time at which the event should be reevaluated.
	 */
	void create_change(const std::shared_ptr<Event> event,
	                   const time::time_t changes_at);

	/**
	 * Get the event queue.
	 *
	 * @return Event queue.
	 */
	const EventQueue &get_queue() const {
		return this->queue;
	}

private:
	/**
	 * Event submitted via \p submit_event() that is not created yet.
	 */
	struct submitted_event {
		std::string eventhandler;
		std::shared_ptr<EventEntity> target;
		std::shared_ptr<State> state;
		time::time_t reference_time;
		EventHandler::param_map params;
	};

	/**
	 * Create all events from the inbox and empty it.
	 *
	 * Must be called with \p mutex held.
	 */
	void drain_inbox();

	/**
	 *  Execute events in the queue with execution time <= a given point in time.
	 *
	 * @param time_until Maximum time until which events are executed.
	 * @param state Global state.
	 *
	 * @returns number of events processed
	 */
	int execute_events(const time::time_t &time_until,
	                   const std::shared_ptr<State> &state);

	/**
	 * Call all the time change functions. This is constant on the state!
	 *
	 * @param state Global state.
	 */
	void update_changes(const std::shared_ptr<State> &state);

	/**
	 * Here we do the bookkeeping of registered event handleres.
	 */
	std::unordered_map<std::string, std::shared_ptr<EventHandler>> classstore;

	/**
	 * All events are enqueued here.
	 */
	EventQueue queue;

	/**
	 * The currently processed event.
	 * This is useful for event cancelations (so one can't cancel itself).
	 */
	std::shared_ptr<Event> active_event;

	/**
	 * Mutex for protecting threaded access.
	 */
	std::recursive_mutex mutex;

	/**
	 * Events submitted from other threads, waiting to be created by \p reach_time().
	 */
	std::vector<submitted_event> inbox;

	/**
	 * Mutex for the inbox only, so submitters never wait for event execution.
	 */
	std::mutex inbox_mutex;
};

} // namespace openage::event
