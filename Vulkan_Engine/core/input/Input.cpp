#include "core/input/Input.h"

#include <array>

#include "core/events/EventBus.h"
#include "core/events/InputEvents.h"
#include "core/events/MouseEvents.h"

namespace Core {

namespace {

constexpr size_t kMaxKeys = 512;
constexpr size_t kMaxMouseButtons = 16;

std::array<bool, kMaxKeys> keyDown{};
std::array<bool, kMaxKeys> keyPressed{};
std::array<bool, kMaxKeys> keyReleased{};

std::array<bool, kMaxMouseButtons> mouseDown{};
std::array<bool, kMaxMouseButtons> mousePressed{};
std::array<bool, kMaxMouseButtons> mouseReleased{};

glm::vec2 mousePos{0.0f, 0.0f};
glm::vec2 mouseDelta{0.0f, 0.0f};
glm::vec2 scrollDelta{0.0f, 0.0f};

bool initialized = false;
bool gameplayInputEnabled = false;

} // namespace

void Input::init(EventBus& eventBus) {
	if (initialized) {
		return;
	}

	eventBus.subscribe(EventType::KeyPressed, [](Event& e) {
		onKeyPressed(static_cast<const KeyPressedEvent&>(e));
	});
	eventBus.subscribe(EventType::KeyReleased, [](Event& e) {
		onKeyReleased(static_cast<const KeyReleasedEvent&>(e));
	});
	eventBus.subscribe(EventType::MouseButtonPressed, [](Event& e) {
		onMouseButtonPressed(static_cast<const MouseButtonPressedEvent&>(e));
	});
	eventBus.subscribe(EventType::MouseButtonReleased, [](Event& e) {
		onMouseButtonReleased(static_cast<const MouseButtonReleasedEvent&>(e));
	});
	eventBus.subscribe(EventType::MouseMoved, [](Event& e) {
		onMouseMoved(static_cast<const MouseMovedEvent&>(e));
	});
	eventBus.subscribe(EventType::MouseScrolled, [](Event& e) {
		onMouseScrolled(static_cast<const MouseScrolledEvent&>(e));
	});

	clearAll();
	initialized = true;
}

void Input::beginFrame() {
	keyPressed.fill(false);
	keyReleased.fill(false);
	mousePressed.fill(false);
	mouseReleased.fill(false);
	mouseDelta = glm::vec2(0.0f, 0.0f);
	scrollDelta = glm::vec2(0.0f, 0.0f);
}

void Input::clearAll() {
	keyDown.fill(false);
	keyPressed.fill(false);
	keyReleased.fill(false);
	mouseDown.fill(false);
	mousePressed.fill(false);
	mouseReleased.fill(false);
	mouseDelta = glm::vec2(0.0f, 0.0f);
	scrollDelta = glm::vec2(0.0f, 0.0f);
}

bool Input::isKeyDown(int keycode) {
	if (!gameplayInputEnabled || !isValidKey(keycode)) {
		return false;
	}
	return keyDown[static_cast<size_t>(keycode)];
}

bool Input::wasKeyPressed(int keycode) {
	if (!gameplayInputEnabled || !isValidKey(keycode)) {
		return false;
	}
	return keyPressed[static_cast<size_t>(keycode)];
}

bool Input::wasKeyReleased(int keycode) {
	if (!gameplayInputEnabled || !isValidKey(keycode)) {
		return false;
	}
	return keyReleased[static_cast<size_t>(keycode)];
}

bool Input::isMouseDown(int button) {
	if (!gameplayInputEnabled || !isValidMouseButton(button)) {
		return false;
	}
	return mouseDown[static_cast<size_t>(button)];
}

bool Input::wasMousePressed(int button) {
	if (!gameplayInputEnabled || !isValidMouseButton(button)) {
		return false;
	}
	return mousePressed[static_cast<size_t>(button)];
}

bool Input::wasMouseReleased(int button) {
	if (!gameplayInputEnabled || !isValidMouseButton(button)) {
		return false;
	}
	return mouseReleased[static_cast<size_t>(button)];
}

glm::vec2 Input::getMousePosition() {
	if (!gameplayInputEnabled) {
		return glm::vec2(0.0f, 0.0f);
	}
	return mousePos;
}

glm::vec2 Input::getMouseDelta() {
	if (!gameplayInputEnabled) {
		return glm::vec2(0.0f, 0.0f);
	}
	return mouseDelta;
}

glm::vec2 Input::getScrollDelta() {
	if (!gameplayInputEnabled) {
		return glm::vec2(0.0f, 0.0f);
	}
	return scrollDelta;
}

void Input::setGameplayInputEnabled(bool enabled) {
	if (!enabled && gameplayInputEnabled) {
		clearAll();
	}
	gameplayInputEnabled = enabled;
}

bool Input::isGameplayInputEnabled() {
	return gameplayInputEnabled;
}

void Input::onKeyPressed(const KeyPressedEvent& event) {
	const int key = event.getKeyCode();
	if (!isValidKey(key)) {
		return;
	}

	const size_t idx = static_cast<size_t>(key);
	if (!keyDown[idx]) {
		keyPressed[idx] = true;
	}
	keyDown[idx] = true;
}

void Input::onKeyReleased(const KeyReleasedEvent& event) {
	const int key = event.getKeyCode();
	if (!isValidKey(key)) {
		return;
	}

	const size_t idx = static_cast<size_t>(key);
	if (keyDown[idx]) {
		keyReleased[idx] = true;
	}
	keyDown[idx] = false;
}

void Input::onMouseButtonPressed(const MouseButtonPressedEvent& event) {
	const int button = event.getMouseButton();
	if (!isValidMouseButton(button)) {
		return;
	}

	const size_t idx = static_cast<size_t>(button);
	if (!mouseDown[idx]) {
		mousePressed[idx] = true;
	}
	mouseDown[idx] = true;
}

void Input::onMouseButtonReleased(const MouseButtonReleasedEvent& event) {
	const int button = event.getMouseButton();
	if (!isValidMouseButton(button)) {
		return;
	}

	const size_t idx = static_cast<size_t>(button);
	if (mouseDown[idx]) {
		mouseReleased[idx] = true;
	}
	mouseDown[idx] = false;
}

void Input::onMouseMoved(const MouseMovedEvent& event) {
	const glm::vec2 newPos(event.getX(), event.getY());
	mouseDelta += (newPos - mousePos);
	mousePos = newPos;
}

void Input::onMouseScrolled(const MouseScrolledEvent& event) {
	scrollDelta += glm::vec2(event.getXOffset(), event.getYOffset());
}

bool Input::isValidKey(int keycode) {
	return keycode >= 0 && static_cast<size_t>(keycode) < kMaxKeys;
}

bool Input::isValidMouseButton(int button) {
	return button >= 0 && static_cast<size_t>(button) < kMaxMouseButtons;
}

} // namespace Core
