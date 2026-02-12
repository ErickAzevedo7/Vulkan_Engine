#include "EventBus.h"

#include "core/events/Event.h"


namespace Core {

void EventBus::subscribe(EventType type, EventCallback callback) {
	subscribers[type].push_back(callback);
}

void EventBus::publish(Event& event) {
	EventType type = event.getEventType();
	if (subscribers.find(type) != subscribers.end()) {
		for (auto& callback : subscribers[type]) {
			callback(event);
			if (event.handled)
				return;
		}
	}
}

} // namespace Core
