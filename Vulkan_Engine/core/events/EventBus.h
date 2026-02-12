#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Event.h"

namespace Core {

class EventBus {
public:
	using EventCallback = std::function<void(Event&)>;

	// Subscribe to a specific event type
	void subscribe(EventType type, EventCallback callback);

	// Unsubscribe (simplified for now, usually requires an ID)
	// void unsubscribe(EventType type, EventCallback callback);

	// Publish an event to listeners
	void publish(Event& event);

private:
	std::unordered_map<EventType, std::vector<EventCallback>> subscribers;
};

} // namespace Core
