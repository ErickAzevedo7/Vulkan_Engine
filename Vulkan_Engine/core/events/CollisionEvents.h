#pragma once

#include <cstdint>
#include <sstream>

#include "Event.h"

namespace Core {

class TriggerEnterEvent : public Event {
public:
	TriggerEnterEvent(uint32_t receiver, uint32_t other)
		: receiverId(receiver), otherId(other) {
	}

	EventType getEventType() const override {
		return EventType::TriggerEnter;
	}

	const char* getName() const override {
		return "TriggerEnterEvent";
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "TriggerEnterEvent: " << receiverId << " <-> " << otherId;
		return ss.str();
	}

	uint32_t getReceiverId() const {
		return receiverId;
	}
	uint32_t getOtherId() const {
		return otherId;
	}

private:
	uint32_t receiverId;
	uint32_t otherId;
};

class TriggerStayEvent : public Event {
public:
	TriggerStayEvent(uint32_t receiver, uint32_t other)
		: receiverId(receiver), otherId(other) {
	}

	EventType getEventType() const override {
		return EventType::TriggerStay;
	}

	const char* getName() const override {
		return "TriggerStayEvent";
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "TriggerStayEvent: " << receiverId << " <-> " << otherId;
		return ss.str();
	}

	uint32_t getReceiverId() const {
		return receiverId;
	}
	uint32_t getOtherId() const {
		return otherId;
	}

private:
	uint32_t receiverId;
	uint32_t otherId;
};

class TriggerExitEvent : public Event {
public:
	TriggerExitEvent(uint32_t receiver, uint32_t other)
		: receiverId(receiver), otherId(other) {
	}

	EventType getEventType() const override {
		return EventType::TriggerExit;
	}

	const char* getName() const override {
		return "TriggerExitEvent";
	}

	std::string toString() const override {
		std::stringstream ss;
		ss << "TriggerExitEvent: " << receiverId << " <-> " << otherId;
		return ss.str();
	}

	uint32_t getReceiverId() const {
		return receiverId;
	}
	uint32_t getOtherId() const {
		return otherId;
	}

private:
	uint32_t receiverId;
	uint32_t otherId;
};

} // namespace Core
