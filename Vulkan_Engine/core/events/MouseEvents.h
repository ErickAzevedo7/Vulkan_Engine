#pragma once

#include <sstream>
#include <string>

#include "Event.h"

namespace Core {

class MouseButtonEvent : public Event {
public:
	inline int getMouseButton() const {
		return button;
	}

protected:
	MouseButtonEvent(int button) : button(button) {
	}
	int button;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
	MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "MouseButtonPressedEvent: " << button;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::MouseButtonPressed;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "MouseButtonPressed";
	}
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
	MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "MouseButtonReleasedEvent: " << button;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::MouseButtonReleased;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "MouseButtonReleased";
	}
};

class MouseMovedEvent : public Event {
public:
	MouseMovedEvent(float x, float y) : mouseX(x), mouseY(y) {
	}

	inline float getX() const {
		return mouseX;
	}
	inline float getY() const {
		return mouseY;
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "MouseMovedEvent: " << mouseX << ", " << mouseY;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::MouseMoved;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "MouseMoved";
	}

private:
	float mouseX, mouseY;
};

class MouseScrolledEvent : public Event {
public:
	MouseScrolledEvent(float xOffset, float yOffset) : xOffset(xOffset), yOffset(yOffset) {
	}

	inline float getXOffset() const {
		return xOffset;
	}
	inline float getYOffset() const {
		return yOffset;
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "MouseScrolledEvent: " << xOffset << ", " << yOffset;
		return ss.str();
	}

	static EventType getStaticType() {
		return EventType::MouseScrolled;
	}
	virtual EventType getEventType() const override {
		return getStaticType();
	}
	virtual const char* getName() const override {
		return "MouseScrolled";
	}

private:
	float xOffset, yOffset;
};
} // namespace Core
