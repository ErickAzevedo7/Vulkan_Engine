#pragma once

#include <renderer/Overlay.h>

#include <string>

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    // Shows the scene, drawn into a view texture the size of the panel. Until the editor has
    // its own camera, the scene is seen from its main camera.
    class ViewportPanel {
    public:
        ViewportPanel(Renderer::Renderer& renderer, UI::UISystem& ui);

        void Create();
        void Destroy();

        // The window's title, with its icon.
        static const char* GetWindowName();

        // `stats` shows in the bottom-left corner; `hasCamera` false shows a note instead of
        // an empty view.
        void Draw(bool* open, const std::string& stats, bool hasCamera);

        // Whether the panel showed this frame, so the scene must be drawn into its texture.
        bool IsVisible() const { return m_Visible; }
        Renderer::ViewTextureID GetTexture() const { return m_Texture; }

    private:
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;
        Renderer::ViewTextureID m_Texture;
        bool m_Visible = false;
    };

} // namespace VKE::Editor
