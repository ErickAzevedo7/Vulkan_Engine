#pragma once

#include <core/System.h>
#include <core/Types.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISwapchain.h>
#include <vector>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE {

    // Throwaway, sandbox-only glue that owns the RHI objects and draws one
    // hardcoded triangle each frame, to exercise the RHI end-to-end. This is
    // NOT a real draw orchestration layer — it belongs in sandbox/, not
    // engine/RHI, and shouldn't be extended into one.
    class TriangleRenderSystem : public System {
    public:
        explicit TriangleRenderSystem(Platform::IWindow& window);

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "TriangleRenderSystem"; }

    private:
        void CreateFramebuffers();
        void RecreateSwapchain();

        Platform::IWindow& m_Window;

        // Declaration order doubles as destruction order (reverse): Device
        // is destroyed last, command buffers/vertex buffer/pipeline first.
        Iunique<RHI::RHIDevice> m_Device;
        Iunique<RHI::RHISwapchain> m_Swapchain;
        Iunique<RHI::RHIRenderPass> m_RenderPass;
        std::vector<Iunique<RHI::RHIFramebuffer>> m_Framebuffers;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        Iunique<RHI::RHIBuffer> m_VertexBuffer;

        // One per frame-in-flight (RHISwapchain::GetFramesInFlightCount()),
        // selected each frame by RHISwapchain::GetCurrentFrameIndex() — its
        // fence is what makes a given slot's buffer safe to re-record.
        std::vector<Iunique<RHI::RHICommandBuffer>> m_CommandBuffers;
    };

} // namespace VKE
