#pragma once

#include <sstream>
#include <string>

#include "Event.h"

namespace Core {

class WindowResizeEvent : public Event {
public:
	WindowResizeEvent(unsigned int width, unsigned int height) : width(width), height(height) {
	}

	unsigned int getWidth() const {
		return width;
	}
	unsigned int getHeight() const {
		return height;
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "WindowResizeEvent: " << width << ", " << height;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::WindowResize;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "WindowResize";
	}

private:
	unsigned int width, height;
};

class WindowCloseEvent : public Event {
public:
	WindowCloseEvent() = default;

	static EventType getStaticType() {
		return EventType::WindowClose;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "WindowClose";
	}
};
} // namespace Core
