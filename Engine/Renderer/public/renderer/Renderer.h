#pragma once

#include <core/System.h>
#include <core/Types.h>
#include <renderer/RenderTypes.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {

    class ForwardPath;
    class RenderScene;

    // Owns the graphics device and draws render scenes to the window.
    class Renderer : public System {
    public:
        Renderer(Platform::IWindow& window, Assets::AssetManager& assets);
        ~Renderer() override;

        void OnInitialize() override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "Renderer"; }

        // Draws `scene` as seen from `view` and presents the frame.
        void Render(const RenderScene& scene, const RenderView& view);

        // Scales the scene's color before it is mapped to displayable range.
        void SetExposure(f32 exposure);

        // Lights that ask for shadows only get them while this is on.
        void SetShadowsEnabled(bool enabled);

    private:
        Platform::IWindow& m_Window;
        Assets::AssetManager& m_Assets;
        f32 m_Exposure = 1.0f;
        bool m_ShadowsEnabled = true;
        Iunique<ForwardPath> m_ForwardPath;
    };

} // namespace VKE::Renderer
