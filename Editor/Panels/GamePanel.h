#pragma once

#include <core/Types.h>
#include <renderer/Overlay.h>

#include <string>

namespace VKE {
class Renderer;
} // namespace VKE

namespace VKE {
class UISystem;
} // namespace VKE

namespace VKE::Editor {

class EditorScene;

// The scene as the game sees it: from its main camera, with nothing of the editor over it.
class GamePanel {
public:
	GamePanel(EditorScene &scene, Renderer &renderer, UISystem &ui);

	void Create();
	void Destroy();

	// The window's title, with its icon.
	static const char *GetWindowName();

	// `stats` shows in the bottom-left corner.
	void Draw(bool *open, const std::string &stats);

	// Whether the scene must be drawn into the panel's texture this frame.
	bool IsVisible() const { return m_Visible; }
	ViewTextureID GetTexture() const { return m_Texture; }

	// Whether the player is on the panel, so the keyboard and the mouse are the game's.
	bool IsFocused() const { return m_Focused; }
	// Where the view starts in the window, in pixels.
	f32 GetViewX() const { return m_ViewX; }
	f32 GetViewY() const { return m_ViewY; }

private:
	EditorScene &m_Scene;
	Renderer &m_Renderer;
	UISystem &m_UI;
	ViewTextureID m_Texture;
	bool m_Visible = false;
	bool m_Focused = false;
	f32 m_ViewX = 0.0f;
	f32 m_ViewY = 0.0f;
};

} // namespace VKE::Editor
