#pragma once

#include "GpuCache.h"
#include "RenderTarget.h"
#include "SkyboxPass.h"
#include "TonemapPass.h"

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

    // Draws a render scene in a single forward pass into an offscreen target,
    // which the tonemap pass then puts on screen.
    class ForwardPath {
    public:
        ForwardPath(Platform::IWindow& window, Assets::AssetManager& assets);
        ~ForwardPath();

        void Render(const RenderScene& scene, const RenderView& view);
        void SetExposure(f32 exposure) { m_Exposure = exposure; }
        void WaitIdle();

    private:
        struct DrawItem {
            const GpuMaterial* Material;
            const GpuMesh* Mesh;
            const Assets::Submesh* Submesh;
            const MeshInstanceDesc* Instance;
        };

        void CreatePipeline();
        void CreateTargets();
        void RecreateSwapchain();
        void CollectDrawItems(const RenderScene& scene);
        void UpdateEnvironment(const RenderScene& scene);
        void WriteFrameUniforms(u32 frameIndex, const RenderScene& scene, const RenderView& view);

        Platform::IWindow& m_Window;

        // Destroyed in reverse declaration order.
        Iunique<RHI::RHIDevice> m_Device;
        Iunique<RHI::RHISwapchain> m_Swapchain;

        Iunique<RHI::RHIRenderPass> m_ScenePass;
        Iunique<RHI::RHIRenderPass> m_OutputPass;
        // One scene target per frame in flight, one output framebuffer per swapchain image.
        std::vector<RenderTarget> m_SceneTargets;
        std::vector<Iunique<RHI::RHIFramebuffer>> m_OutputFramebuffers;

        Iunique<RHI::RHIBindGroupLayout> m_FrameLayout;
        Iunique<RHI::RHIBindGroupLayout> m_EnvironmentLayout;
        Iunique<RHI::RHIBindGroupLayout> m_MaterialLayout;
        Iunique<RHI::RHIPipeline> m_Pipeline;

        Iunique<GpuCache> m_GpuCache;
        Iunique<SkyboxPass> m_SkyboxPass;
        Iunique<TonemapPass> m_TonemapPass;

        // Rebuilt whenever the scene points at a different environment.
        Iunique<RHI::RHISampler> m_EnvironmentSampler;
        Iunique<RHI::RHIBindGroup> m_EnvironmentGroup;
        const GpuEnvironment* m_BoundEnvironmentMaps = nullptr;

        // One per frame in flight.
        std::vector<Iunique<RHI::RHIBuffer>> m_FrameUniformBuffers;
        std::vector<Iunique<RHI::RHIBindGroup>> m_FrameGroups;
        std::vector<Iunique<RHI::RHICommandBuffer>> m_CommandBuffers;

        std::vector<DrawItem> m_DrawItems;
        f32 m_Exposure = 1.0f;
        bool m_LightLimitWarned = false;
    };

} // namespace VKE::Renderer
