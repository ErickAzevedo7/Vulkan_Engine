#pragma once

#include <core/System.h>
#include <core/Types.h>
#include <renderer/Overlay.h>
#include <renderer/RenderTypes.h>
#include <renderer/WorldDraw.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {

    class FrameRenderer;
    class RenderScene;

    // Owns the graphics device and builds each frame from parts: scenes drawn to the window
    // or into view textures, then a 2D layer over the window.
    //
    //   if (renderer.BeginFrame()) {
    //       renderer.DrawScene(scene, view, viewport);   // into a view texture
    //       renderer.DrawOverlay(uiList);                // the UI shows that texture
    //       renderer.EndFrame();
    //   }
    class Renderer : public System {
    public:
        Renderer(Platform::IWindow& window, Assets::AssetManager& assets);
        ~Renderer() override;

        void OnInitialize() override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "Renderer"; }

        // Starts recording a frame. False when there is nothing to draw to (a minimized
        // window, or a swapchain that had to be rebuilt): skip this frame's drawing then.
        bool BeginFrame();

        // Draws `scene` as seen from `view`. Without a target it fills the window, under
        // the 2D layer; the projection uses the target's aspect ratio. A scene can be
        // drawn several times a frame, into different targets. `world` is drawn over the
        // scene, into view textures only.
        void DrawScene(const RenderScene& scene, const RenderView& view, ViewTextureID target = {},
                       const WorldDrawList* world = nullptr);

        // Finds the mesh that covers pixel (x, y) of a view texture, counted from its top left,
        // the next time a scene is drawn into it. The answer takes a few frames to come back
        // from the GPU; a new request replaces one not yet drawn.
        void RequestPick(ViewTextureID target, u32 x, u32 y);
        // True once, when the answer to the last request is in. `mesh` is then the instance
        // at that pixel, or invalid for none.
        bool TakePickResult(ViewTextureID target, MeshInstanceID& mesh);

        // Adds a list to the 2D layer, drawn over the window after any earlier list.
        void DrawOverlay(const OverlayDrawList& list);

        // Draws the 2D layer and presents the frame.
        void EndFrame();

        // Draws one frame with just `scene` in the window.
        void Render(const RenderScene& scene, const RenderView& view);

        // Starts out black. Changing the size keeps the ID; the contents are cleared.
        ViewTextureID CreateViewTexture(u32 width, u32 height);
        void ResizeViewTexture(ViewTextureID id, u32 width, u32 height);
        void DestroyViewTexture(ViewTextureID id);

        // `pixels` holds width * height RGBA bytes, top row first, sRGB-encoded like CSS colors.
        OverlayImageID CreateOverlayImage(u32 width, u32 height, const void* pixels);
        void DestroyOverlayImage(OverlayImageID id);

        // Scales the scene's color before it is mapped to displayable range.
        void SetExposure(f32 exposure);

        // Lights that ask for shadows only get them while this is on.
        void SetShadowsEnabled(bool enabled);

    private:
        Platform::IWindow& m_Window;
        Assets::AssetManager& m_Assets;
        f32 m_Exposure = 1.0f;
        bool m_ShadowsEnabled = true;
        Iunique<FrameRenderer> m_Frame;
    };

} // namespace VKE::Renderer
