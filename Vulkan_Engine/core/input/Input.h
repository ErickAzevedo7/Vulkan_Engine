#pragma once

#include "glm/ext/vector_float2.hpp"

namespace Core {

class EventBus;
class KeyPressedEvent;
class KeyReleasedEvent;
class MouseButtonPressedEvent;
class MouseButtonReleasedEvent;
class MouseMovedEvent;
class MouseScrolledEvent;

class Input {
public:
	static void init(EventBus& eventBus);
	static void beginFrame();
	static void clearAll();

	static bool isKeyDown(int keycode);
	static bool wasKeyPressed(int keycode);
	static bool wasKeyReleased(int keycode);

	static bool isMouseDown(int button);
	static bool wasMousePressed(int button);
	static bool wasMouseReleased(int button);

	static glm::vec2 getMousePosition();
	static glm::vec2 getMouseDelta();
	static glm::vec2 getScrollDelta();

	static void setGameplayInputEnabled(bool enabled);
	static bool isGameplayInputEnabled();

private:
	static void onKeyPressed(const KeyPressedEvent& event);
	static void onKeyReleased(const KeyReleasedEvent& event);
	static void onMouseButtonPressed(const MouseButtonPressedEvent& event);
	static void onMouseButtonReleased(const MouseButtonReleasedEvent& event);
	static void onMouseMoved(const MouseMovedEvent& event);
	static void onMouseScrolled(const MouseScrolledEvent& event);

	static bool isValidKey(int keycode);
	static bool isValidMouseButton(int button);
};

} // namespace Core
