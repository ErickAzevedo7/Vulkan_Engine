#pragma once

#include <sstream>
#include <string>

#include "Event.h"

namespace Core {

class KeyEvent : public Event {
public:
	inline int getKeyCode() const {
		return keyCode;
	}

protected:
	KeyEvent(int keycode) : keyCode(keycode) {
	}
	int keyCode;
};

class KeyPressedEvent : public KeyEvent {
public:
	KeyPressedEvent(int keycode, int repeatCount) : KeyEvent(keycode), repeatCount(repeatCount) {
	}

	inline int getRepeatCount() const {
		return repeatCount;
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "KeyPressedEvent: " << keyCode << " (" << repeatCount << " repeats)";
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::KeyPressed;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "KeyPressed";
	}

private:
	int repeatCount;
};

class KeyReleasedEvent : public KeyEvent {
public:
	KeyReleasedEvent(int keycode) : KeyEvent(keycode) {
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "KeyReleasedEvent: " << keyCode;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::KeyReleased;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "KeyReleased";
	}
};
} // namespace Core
