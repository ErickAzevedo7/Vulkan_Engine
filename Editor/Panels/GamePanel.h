#pragma once

#include <core/Types.h>
#include <renderer/Overlay.h>

#include <string>

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    class EditorScene;

    // The scene as the game sees it: from its main camera, with nothing of the editor over it.
    class GamePanel {
    public:
        GamePanel(EditorScene& scene, Renderer::Renderer& renderer, UI::UISystem& ui);

        void Create();
        void Destroy();

        // The window's title, with its icon.
        static const char* GetWindowName();

        // `stats` shows in the bottom-left corner.
        void Draw(bool* open, const std::string& stats);

        // Whether the scene must be drawn into the panel's texture this frame.
        bool IsVisible() const { return m_Visible; }
        Renderer::ViewTextureID GetTexture() const { return m_Texture; }

    private:
        EditorScene& m_Scene;
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;
        Renderer::ViewTextureID m_Texture;
        bool m_Visible = false;
    };

} // namespace VKE::Editor
