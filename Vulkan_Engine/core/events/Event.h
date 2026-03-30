#pragma once

#include <ostream>
#include <string>

namespace Core {

enum class EventType {
	None = 0,
	WindowClose,
	WindowResize,
	KeyPressed,
	KeyReleased,
	MouseButtonPressed,
	MouseButtonReleased,
	MouseMoved,
	MouseScrolled,
	AppTick,
	AppUpdate,
	AppRender,
	TriggerEnter,
	TriggerStay,
	TriggerExit
};

// Base class for all events
class Event {
public:
	bool handled = false;

	virtual ~Event() = default;
	virtual EventType getEventType() const = 0;
	virtual const char* getName() const = 0;
	virtual std::string toString() const {
		return getName();
	}

	inline bool isInCategory(EventType category) {
		return getEventType() == category;
	}
};

// Generic stream insertion for events
inline std::ostream& operator<<(std::ostream& os, const Event& e) {
	return os << e.toString();
}

} // namespace Core
