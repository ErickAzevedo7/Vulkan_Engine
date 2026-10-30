#pragma once

#include <core/System.h>
#include <core/Types.h>
#include <renderer/Overlay.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiContext;

namespace VKE::Platform {
    class IWindow;
    class IInput;
} // namespace VKE::Platform

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {

    // Runs Dear ImGui over the engine: feeds it the window's input, builds its fonts and
    // theme from the design system, and hands what it draws to the renderer's 2D layer.
    // ImGui is set up with docking and is ready for calls from every system updated after
    // this one, until Render() closes the frame:
    //
    //   ImGui::Begin("Panel"); ... ImGui::End();   // any system, after UISystem::OnUpdate
    //   if (renderer.BeginFrame()) { ...; ui.Render(); renderer.EndFrame(); }
    //   else ui.Render();                             // closes the frame anyway
    class UISystem : public System {
    public:
        UISystem(Platform::IWindow& window, Platform::IInput& input, Renderer::Renderer& renderer);
        ~UISystem() override;

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "UISystem"; }

        // Ends the frame's UI and adds it to the renderer's 2D layer when a renderer frame
        // is open. Call once per frame, after every UI call.
        void Render();

        // The ImGui texture ID (ImTextureID) that shows `texture`, e.g. in ImGui::Image().
        u64 GetTextureID(const Renderer::OverlayTexture& texture);

        // The texture of the font atlas, where the glyphs of every font are (ImFontGlyph UVs).
        Renderer::OverlayTexture GetFontTexture() const { return Renderer::OverlayTexture::FromImage(m_FontAtlas); }

        // Whether the UI is using the mouse or keyboard this frame, so the scene should not.
        bool WantsMouse() const;
        bool WantsKeyboard() const;

        // Where ImGui keeps window positions and the docking layout. Empty keeps nothing.
        void SetLayoutFile(const std::filesystem::path& file);

    private:
        // The font files and glyph ranges the atlas reads from, kept for as long as it lives.
        struct FontStorage;

        struct TextureKeyHash {
            size_t operator()(const Renderer::OverlayTexture& texture) const;
        };

        void BuildFonts();
        void FeedInput(FrameTime ft);
        void ApplyCursor();
        void BuildDrawList();

        Platform::IWindow& m_Window;
        Platform::IInput& m_Input;
        Renderer::Renderer& m_Renderer;

        ImGuiContext* m_Context = nullptr;
        Iunique<FontStorage> m_FontStorage;
        Renderer::OverlayImageID m_FontAtlas;
        std::string m_LayoutFile;
        // Kept alive for ImGui, which reads clipboard text through a pointer.
        std::string m_ClipboardText;
        bool m_FrameOpen = false;

        // ImTextureID n is m_Textures[n - 1]; 0 is no texture.
        std::vector<Renderer::OverlayTexture> m_Textures;
        std::unordered_map<Renderer::OverlayTexture, u64, TextureKeyHash> m_TextureIDs;

        Renderer::OverlayDrawList m_DrawList;
    };

} // namespace VKE::UI
