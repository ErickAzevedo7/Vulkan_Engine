#pragma once

#include "GpuCache.h"

#include <core/Types.h>
#include <renderer/RenderTypes.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIPipeline.h>
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

    // Draws a render scene in a single forward pass straight to the window.
    class ForwardPath {
    public:
        ForwardPath(Platform::IWindow& window, Assets::AssetManager& assets);
        ~ForwardPath();

        void Render(const RenderScene& scene, const RenderView& view);
        void WaitIdle();

    private:
        struct DrawItem {
            const GpuMaterial* Material;
            const GpuMesh* Mesh;
            const Assets::Submesh* Submesh;
            const MeshInstanceDesc* Instance;
        };

        void CreatePipeline();
        void CreateSwapchainTargets();
        void RecreateSwapchain();
        void CollectDrawItems(const RenderScene& scene);
        void WriteFrameUniforms(u32 frameIndex, const RenderScene& scene, const RenderView& view);

        Platform::IWindow& m_Window;

        // Destroyed in reverse declaration order.
        Iunique<RHI::RHIDevice> m_Device;
        Iunique<RHI::RHISwapchain> m_Swapchain;
        Iunique<RHI::RHIRenderPass> m_RenderPass;
        std::vector<Iunique<RHI::RHITexture>> m_DepthTextures;
        std::vector<Iunique<RHI::RHIFramebuffer>> m_Framebuffers;

        Iunique<RHI::RHIBindGroupLayout> m_FrameLayout;
        Iunique<RHI::RHIBindGroupLayout> m_MaterialLayout;
        Iunique<RHI::RHIPipeline> m_Pipeline;

        Iunique<GpuCache> m_GpuCache;

        // One per frame in flight.
        std::vector<Iunique<RHI::RHIBuffer>> m_FrameUniformBuffers;
        std::vector<Iunique<RHI::RHIBindGroup>> m_FrameGroups;
        std::vector<Iunique<RHI::RHICommandBuffer>> m_CommandBuffers;

        std::vector<DrawItem> m_DrawItems;
        bool m_LightLimitWarned = false;
    };

} // namespace VKE::Renderer
