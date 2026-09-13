#include "TriangleRenderSystem.h"

#include <core/Log.h>
#include <math/MathTypes.h>
#include <platform/Window.h>

#include <cstddef>

namespace VKE {

    namespace {
        struct Vertex {
            Vec3 Position;
            Vec3 Color;
        };

        // CullMode::None below sidesteps needing to reason about NDC
        // winding order for these three fixed points.
        const std::vector<Vertex> kVertices = {
            {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };
    } // namespace

    TriangleRenderSystem::TriangleRenderSystem(Platform::IWindow& window) : m_Window(window) {
    }

    void TriangleRenderSystem::OnInitialize() {
        m_Device = RHI::RHIDevice::Create(m_Window);
        m_Swapchain = RHI::RHISwapchain::Create(*m_Device, m_Window);

        RHI::RenderPassDesc renderPassDesc;
        RHI::AttachmentDesc colorAttachment;
        colorAttachment.Format = m_Swapchain->GetColorFormat();
        colorAttachment.Clear = true;
        colorAttachment.PresentAfter = true;
        renderPassDesc.ColorAttachments = {colorAttachment};
        m_RenderPass = RHI::RHIRenderPass::Create(*m_Device, renderPassDesc);

        CreateFramebuffers();

        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = "shaders/triangle.vert.spv";
        pipelineDesc.FragmentShaderPath = "shaders/triangle.frag.spv";
        pipelineDesc.VertexStride = sizeof(Vertex);
        pipelineDesc.VertexAttributes = {
            {0, RHI::Format::R32G32B32_SFLOAT, offsetof(Vertex, Position)},
            {1, RHI::Format::R32G32B32_SFLOAT, offsetof(Vertex, Color)},
        };
        pipelineDesc.CullMode = RHI::CullMode::None;
        m_Pipeline = RHI::RHIPipeline::Create(*m_Device, *m_RenderPass, pipelineDesc);

        m_VertexBuffer = RHI::RHIBuffer::Create(*m_Device, kVertices.data(), kVertices.size() * sizeof(Vertex),
                                                 RHI::BufferUsage::Vertex);

        m_CommandBuffers.reserve(m_Swapchain->GetFramesInFlightCount());
        for (u32 i = 0; i < m_Swapchain->GetFramesInFlightCount(); ++i) {
            m_CommandBuffers.push_back(RHI::RHICommandBuffer::Create(*m_Device));
        }

        VKE_CORE_INFO("TriangleRenderSystem initialized");
    }

    void TriangleRenderSystem::CreateFramebuffers() {
        m_Framebuffers.clear();
        m_Framebuffers.reserve(m_Swapchain->GetImageCount());
        for (u32 i = 0; i < m_Swapchain->GetImageCount(); ++i) {
            RHI::FramebufferDesc fbDesc;
            fbDesc.AttachmentViews = {m_Swapchain->GetImageView(i)};
            fbDesc.Width = m_Swapchain->GetWidth();
            fbDesc.Height = m_Swapchain->GetHeight();
            m_Framebuffers.push_back(RHI::RHIFramebuffer::Create(*m_Device, *m_RenderPass, fbDesc));
        }
    }

    void TriangleRenderSystem::RecreateSwapchain() {
        // Must wait for the GPU to finish with the old framebuffers before
        // destroying them. RHISwapchain::Recreate() also waits idle
        // internally, but only after it starts tearing down the swapchain —
        // too late for framebuffers, which it doesn't own.
        m_Device->WaitIdle();

        // Framebuffers reference swapchain image views directly, so they must
        // be torn down before Recreate() destroys those views, and rebuilt
        // against the new ones — the render pass itself doesn't need
        // recreating (the swapchain's color format doesn't change on resize).
        m_Framebuffers.clear();
        m_Swapchain->Recreate();
        CreateFramebuffers();
    }

    void TriangleRenderSystem::OnUpdate(FrameTime /*ft*/) {
        // Skip drawing while minimized (0x0 framebuffer) rather than trying
        // to recreate a zero-sized swapchain — matches the Platform module's
        // poll-based design (no blocking event wait here).
        if (m_Window.IsMinimized()) return;

        if (m_Window.ConsumeResizedFlag()) {
            RecreateSwapchain();
        }

        u32 imageIndex;
        if (!m_Swapchain->AcquireNextImage(imageIndex)) {
            RecreateSwapchain();
            return;
        }

        RHI::ClearValue clear;
        clear.R = 0.02f;
        clear.G = 0.02f;
        clear.B = 0.05f;
        clear.A = 1.0f;

        // GetCurrentFrameIndex() is only valid between AcquireNextImage()
        // and SubmitAndPresent() — its fence is what makes this slot's
        // command buffer safe to re-record.
        auto& cmd = *m_CommandBuffers[m_Swapchain->GetCurrentFrameIndex()];

        cmd.Begin();
        cmd.BeginRenderPass(*m_RenderPass, *m_Framebuffers[imageIndex], clear);
        cmd.BindPipeline(*m_Pipeline);
        cmd.BindVertexBuffer(*m_VertexBuffer);
        cmd.Draw(static_cast<u32>(kVertices.size()), 1, 0, 0);
        cmd.EndRenderPass();
        cmd.End();

        if (!m_Swapchain->SubmitAndPresent(cmd)) {
            RecreateSwapchain();
        }
    }

    void TriangleRenderSystem::OnShutdown() {
        // Resources are torn down by member destructors right after this
        // returns (declaration order in TriangleRenderSystem.h doubles as
        // destruction order) — this just ensures no GPU work is still
        // in-flight when that teardown starts.
        m_Device->WaitIdle();
        VKE_CORE_INFO("TriangleRenderSystem shutdown");
    }

} // namespace VKE
