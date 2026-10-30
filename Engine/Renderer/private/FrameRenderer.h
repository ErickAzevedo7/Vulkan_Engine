#pragma once

#include "ForwardPath.h"
#include "OverlayPass.h"
#include "PickPass.h"
#include "WorldPass.h"

#include <core/SlotMap.h>
#include <core/Types.h>
#include <renderer/Overlay.h>
#include <renderer/RenderTypes.h>
#include <renderer/WorldDraw.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISwapchain.h>
#include <rhi/RHITexture.h>
#include <vector>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {

    class RenderScene;

    // A view texture's GPU side: the scene targets it draws through, and the displayable
    // color the 2D layer samples, one per frame in flight; and what picking in it needs.
    struct ViewTexture {
        Iunique<SceneTargets> Scene;
        std::vector<Iunique<RHI::RHITexture>> Colors;
        std::vector<Iunique<RHI::RHIFramebuffer>> Framebuffers;
        // The color with the scene's depth, for world draw lists.
        std::vector<Iunique<RHI::RHIFramebuffer>> WorldFramebuffers;
        u32 Width = 0;
        u32 Height = 0;

        // Made on the first request.
        Iunique<PickTarget> Pick;
        // Waiting for the next DrawScene.
        bool PickRequested = false;
        u32 PickX = 0;
        u32 PickY = 0;
        // Drawn in frame PickFrame, and not read yet.
        bool PickInFlight = false;
        u64 PickFrame = 0;
    };

    struct OverlayImage {
        Iunique<RHI::RHITexture> Texture;
    };

    // The renderer's working parts: the device and swapchain, the frame being recorded, the
    // view textures and overlay images, and the passes that fill them. See Renderer.h.
    class FrameRenderer {
    public:
        FrameRenderer(Platform::IWindow& window, Assets::AssetManager& assets);
        ~FrameRenderer();

        bool BeginFrame();
        void DrawScene(const RenderScene& scene, const RenderView& view, ViewTextureID target,
                       const WorldDrawList* world);
        void RequestPick(ViewTextureID target, u32 x, u32 y);
        bool TakePickResult(ViewTextureID target, MeshInstanceID& mesh);
        void DrawOverlay(const OverlayDrawList& list);
        void EndFrame();

        ViewTextureID CreateViewTexture(u32 width, u32 height);
        void ResizeViewTexture(ViewTextureID id, u32 width, u32 height);
        void DestroyViewTexture(ViewTextureID id);

        OverlayImageID CreateOverlayImage(u32 width, u32 height, const void* pixels);
        void DestroyOverlayImage(OverlayImageID id);

        void SetExposure(f32 exposure) { m_ForwardPath->SetExposure(exposure); }
        void SetShadowsEnabled(bool enabled) { m_ForwardPath->SetShadowsEnabled(enabled); }
        void WaitIdle();

    private:
        // Resources dropped while a recorded frame may still use them, freed once that
        // frame is known to be done.
        struct Retired {
            u64 Frame = 0;
            ViewTexture View;
            OverlayImage Image;
        };

        void CreateWindowTargets();
        void RecreateSwapchain();
        ViewTexture MakeViewTexture(u32 width, u32 height);
        void Retire(ViewTexture view);
        void Retire(OverlayImage image);
        void ReleaseRetired(bool all);
        RHI::RHITexture* ResolveOverlayTexture(const OverlayTexture& texture);

        Platform::IWindow& m_Window;

        // Destroyed in reverse declaration order.
        Iunique<RHI::RHIDevice> m_Device;
        Iunique<RHI::RHISwapchain> m_Swapchain;
        // Draws into the swapchain image, presented afterwards.
        Iunique<RHI::RHIRenderPass> m_OutputPass;
        // Draws into a view texture's color, sampled afterwards.
        Iunique<RHI::RHIRenderPass> m_ViewPass;
        // Draws over a view texture's color, with the scene's depth.
        Iunique<RHI::RHIRenderPass> m_WorldRenderPass;

        Iunique<ForwardPath> m_ForwardPath;
        Iunique<OverlayPass> m_OverlayPass;
        Iunique<WorldPass> m_WorldPass;
        Iunique<PickPass> m_PickPass;

        // The scene drawn straight to the window, at the swapchain's size.
        Iunique<SceneTargets> m_WindowScene;
        // One per swapchain image.
        std::vector<Iunique<RHI::RHIFramebuffer>> m_OutputFramebuffers;
        // One per frame in flight.
        std::vector<Iunique<RHI::RHICommandBuffer>> m_CommandBuffers;

        SlotMap<ViewTexture> m_Views;
        SlotMap<OverlayImage> m_Images;
        std::vector<Retired> m_Retired;

        // The first m_OverlayListCount are this frame's lists; all keep their storage between frames.
        std::vector<OverlayDrawList> m_OverlayLists;
        size_t m_OverlayListCount = 0;

        // Counts frames that began, to know when retired resources are free to go.
        u64 m_FrameNumber = 0;
        u32 m_FrameIndex = 0;
        u32 m_ImageIndex = 0;
        bool m_InFrame = false;
        bool m_WindowSceneDrawn = false;
        // True when the swapchain's format does not encode sRGB by itself.
        bool m_OutputNeedsEncoding = true;
    };

} // namespace VKE::Renderer
