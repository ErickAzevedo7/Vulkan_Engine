#include "FrameRenderer.h"

#include <core/Assert.h>
#include <core/Log.h>
#include <platform/Window.h>

#include <algorithm>

namespace VKE::Renderer {

    namespace {
        // View textures store sRGB-encoded values, which the 2D layer blends as they are.
        constexpr RHI::Format kViewColorFormat = RHI::Format::R8G8B8A8_UNORM;

        bool IsSrgbFormat(RHI::Format format) {
            return format == RHI::Format::B8G8R8A8_SRGB || format == RHI::Format::R8G8B8A8_SRGB;
        }
    } // namespace

    FrameRenderer::FrameRenderer(Platform::IWindow& window, Assets::AssetManager& assets) : m_Window(window) {
        m_Device = RHI::RHIDevice::Create(m_Window);
        m_Swapchain = RHI::RHISwapchain::Create(*m_Device, m_Window);

        m_OutputNeedsEncoding = !IsSrgbFormat(m_Swapchain->GetColorFormat());
        if (!m_OutputNeedsEncoding) {
            VKE_CORE_WARN("Renderer: the window only offers an sRGB swapchain format; the 2D layer will blend "
                          "in linear space and look slightly different from its design");
        }

        RHI::RenderPassDesc outputPassDesc;
        RHI::AttachmentDesc outputColor;
        outputColor.Format = m_Swapchain->GetColorFormat();
        outputColor.PresentAfter = true;
        outputPassDesc.ColorAttachments = {outputColor};
        m_OutputPass = RHI::RHIRenderPass::Create(*m_Device, outputPassDesc);

        RHI::RenderPassDesc viewPassDesc;
        RHI::AttachmentDesc viewColor;
        viewColor.Format = kViewColorFormat;
        viewColor.SampledAfter = true;
        viewPassDesc.ColorAttachments = {viewColor};
        m_ViewPass = RHI::RHIRenderPass::Create(*m_Device, viewPassDesc);

        RHI::RenderPassDesc worldPassDesc;
        RHI::AttachmentDesc worldColor = viewColor;
        worldColor.Clear = false;
        worldPassDesc.ColorAttachments = {worldColor};
        worldPassDesc.HasDepthAttachment = true;
        worldPassDesc.DepthAttachment.Format = m_Device->GetDepthFormat();
        worldPassDesc.DepthAttachment.IsDepth = true;
        worldPassDesc.DepthAttachment.Clear = false;
        m_WorldRenderPass = RHI::RHIRenderPass::Create(*m_Device, worldPassDesc);

        u32 framesInFlight = m_Swapchain->GetFramesInFlightCount();
        m_ForwardPath = MakeIunique<ForwardPath>(*m_Device, assets, framesInFlight);
        m_OverlayPass = MakeIunique<OverlayPass>(*m_Device, *m_OutputPass, framesInFlight);
        m_WorldPass = MakeIunique<WorldPass>(*m_Device, *m_WorldRenderPass, framesInFlight);
        m_PickPass = MakeIunique<PickPass>(*m_Device, m_ForwardPath->GetGpuCache());

        CreateWindowTargets();

        for (u32 i = 0; i < framesInFlight; ++i) {
            m_CommandBuffers.push_back(RHI::RHICommandBuffer::Create(*m_Device));
        }
    }

    FrameRenderer::~FrameRenderer() {
        m_Device->WaitIdle();
        ReleaseRetired(true);
    }

    void FrameRenderer::WaitIdle() {
        m_Device->WaitIdle();
    }

    void FrameRenderer::CreateWindowTargets() {
        m_OutputFramebuffers.clear();

        u32 width = m_Swapchain->GetWidth();
        u32 height = m_Swapchain->GetHeight();

        m_WindowScene = m_ForwardPath->CreateTargets(width, height, "WindowScene");

        for (u32 i = 0; i < m_Swapchain->GetImageCount(); ++i) {
            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {m_Swapchain->GetImageView(i)};
            framebufferDesc.Width = width;
            framebufferDesc.Height = height;
            m_OutputFramebuffers.push_back(RHI::RHIFramebuffer::Create(*m_Device, *m_OutputPass, framebufferDesc));
        }
    }

    void FrameRenderer::RecreateSwapchain() {
        // Framebuffers reference the swapchain views, so release them before recreating.
        m_Device->WaitIdle();
        m_OutputFramebuffers.clear();
        m_WindowScene.reset();
        m_Swapchain->Recreate();
        CreateWindowTargets();
    }

    bool FrameRenderer::BeginFrame() {
        VKE_CORE_ASSERT(!m_InFrame, "BeginFrame called twice without EndFrame");
        if (m_Window.IsMinimized()) return false;

        if (m_Window.ConsumeResizedFlag()) {
            RecreateSwapchain();
        }
        if (!m_Swapchain->AcquireNextImage(m_ImageIndex)) {
            RecreateSwapchain();
            return false;
        }

        // Acquiring waited for the frame that last used this slot.
        ++m_FrameNumber;
        m_FrameIndex = m_Swapchain->GetCurrentFrameIndex();
        ReleaseRetired(false);
        m_ForwardPath->GetGpuCache().BeginFrame(m_FrameNumber);
        m_WorldPass->BeginFrame(m_FrameIndex);

        m_InFrame = true;
        m_WindowSceneDrawn = false;
        m_OverlayListCount = 0;
        m_CommandBuffers[m_FrameIndex]->Begin();
        return true;
    }

    void FrameRenderer::DrawScene(const RenderScene& scene, const RenderView& view, ViewTextureID target,
                                  const WorldDrawList* world) {
        if (!m_InFrame) return;
        RHI::RHICommandBuffer& cmd = *m_CommandBuffers[m_FrameIndex];

        if (!target.IsValid()) {
            m_ForwardPath->Record(cmd, m_FrameIndex, scene, view, *m_WindowScene);
            m_WindowSceneDrawn = true;
            return;
        }

        ViewTexture* viewTexture = m_Views.Get(target);
        if (!viewTexture) {
            VKE_CORE_WARN("Renderer::DrawScene: the view texture no longer exists");
            return;
        }
        m_ForwardPath->Record(cmd, m_FrameIndex, scene, view, *viewTexture->Scene);

        cmd.BeginRenderPass(*m_ViewPass, *viewTexture->Framebuffers[m_FrameIndex], RHI::ClearValue{});
        m_ForwardPath->Resolve(cmd, m_FrameIndex, *viewTexture->Scene, *m_ViewPass, true);
        cmd.EndRenderPass();

        f32 aspect = static_cast<f32>(viewTexture->Width) / static_cast<f32>(viewTexture->Height);
        Mat4 viewProjection = view.GetProjection(aspect) * view.View;

        if (world && !world->Indices.empty()) {
            cmd.BeginRenderPass(*m_WorldRenderPass, *viewTexture->WorldFramebuffers[m_FrameIndex], RHI::ClearValue{});
            m_WorldPass->Record(cmd, m_FrameIndex, *world, viewProjection, view.Position, viewTexture->Width,
                                viewTexture->Height,
                                [this](const OverlayTexture& texture) { return ResolveOverlayTexture(texture); });
            cmd.EndRenderPass();
        }

        // A target an earlier frame still copies into waits until its result is taken.
        if (viewTexture->PickRequested && !viewTexture->PickInFlight) {
            if (!viewTexture->Pick) {
                viewTexture->Pick = m_PickPass->CreateTarget(viewTexture->Width, viewTexture->Height);
            }
            m_PickPass->Record(cmd, scene, viewProjection, *viewTexture->Pick, viewTexture->PickX, viewTexture->PickY);
            viewTexture->PickRequested = false;
            viewTexture->PickInFlight = true;
            viewTexture->PickFrame = m_FrameNumber;
        }
    }

    void FrameRenderer::RequestPick(ViewTextureID target, u32 x, u32 y) {
        ViewTexture* viewTexture = m_Views.Get(target);
        if (!viewTexture || x >= viewTexture->Width || y >= viewTexture->Height) return;
        viewTexture->PickRequested = true;
        viewTexture->PickX = x;
        viewTexture->PickY = y;
    }

    bool FrameRenderer::TakePickResult(ViewTextureID target, MeshInstanceID& mesh) {
        ViewTexture* viewTexture = m_Views.Get(target);
        if (!viewTexture || !viewTexture->PickInFlight) return false;
        // Acquiring a frame waits for the frame that last used its slot.
        if (viewTexture->PickFrame + m_Swapchain->GetFramesInFlightCount() > m_FrameNumber) return false;

        mesh = PickPass::ReadResult(*viewTexture->Pick);
        viewTexture->PickInFlight = false;
        return true;
    }

    void FrameRenderer::DrawOverlay(const OverlayDrawList& list) {
        if (!m_InFrame) return;
        if (m_OverlayListCount == m_OverlayLists.size()) {
            m_OverlayLists.emplace_back();
        }
        OverlayDrawList& copy = m_OverlayLists[m_OverlayListCount++];
        copy.Vertices.assign(list.Vertices.begin(), list.Vertices.end());
        copy.Indices.assign(list.Indices.begin(), list.Indices.end());
        copy.Commands.assign(list.Commands.begin(), list.Commands.end());
    }

    void FrameRenderer::EndFrame() {
        if (!m_InFrame) return;
        m_InFrame = false;
        RHI::RHICommandBuffer& cmd = *m_CommandBuffers[m_FrameIndex];

        cmd.BeginRenderPass(*m_OutputPass, *m_OutputFramebuffers[m_ImageIndex], RHI::ClearValue{});
        if (m_WindowSceneDrawn) {
            m_ForwardPath->Resolve(cmd, m_FrameIndex, *m_WindowScene, *m_OutputPass, m_OutputNeedsEncoding);
        }
        m_OverlayPass->Record(cmd, m_FrameIndex, m_OverlayLists.data(), m_OverlayListCount, m_Swapchain->GetWidth(),
                              m_Swapchain->GetHeight(), !m_OutputNeedsEncoding,
                              [this](const OverlayTexture& texture) { return ResolveOverlayTexture(texture); });
        cmd.EndRenderPass();
        cmd.End();

        if (!m_Swapchain->SubmitAndPresent(cmd)) {
            RecreateSwapchain();
        }
    }

    RHI::RHITexture* FrameRenderer::ResolveOverlayTexture(const OverlayTexture& texture) {
        switch (texture.Type) {
            case OverlayTexture::Kind::Image: {
                OverlayImage* image = m_Images.Get({texture.Index, texture.Generation});
                return image ? image->Texture.get() : nullptr;
            }
            case OverlayTexture::Kind::View: {
                ViewTexture* view = m_Views.Get({texture.Index, texture.Generation});
                return view ? view->Colors[m_FrameIndex].get() : nullptr;
            }
            case OverlayTexture::Kind::Asset: {
                Handle<Assets::TextureData> asset{texture.Index, texture.Generation};
                return asset.IsValid() ? &m_ForwardPath->GetGpuCache().GetTexture(asset) : nullptr;
            }
            default: return nullptr;
        }
    }

    ViewTexture FrameRenderer::MakeViewTexture(u32 width, u32 height) {
        width = std::max(width, 1u);
        height = std::max(height, 1u);

        ViewTexture viewTexture;
        viewTexture.Width = width;
        viewTexture.Height = height;
        viewTexture.Scene = m_ForwardPath->CreateTargets(width, height, "ViewScene");

        for (size_t i = 0; i < viewTexture.Scene->Targets.size(); ++i) {
            RHI::TextureDesc colorDesc;
            colorDesc.Width = width;
            colorDesc.Height = height;
            colorDesc.Format = kViewColorFormat;
            colorDesc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            colorDesc.DebugName = "ViewColor";
            viewTexture.Colors.push_back(RHI::RHITexture::Create(*m_Device, colorDesc));

            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {viewTexture.Colors.back()->GetAttachmentView(0, 0)};
            framebufferDesc.Width = width;
            framebufferDesc.Height = height;
            viewTexture.Framebuffers.push_back(RHI::RHIFramebuffer::Create(*m_Device, *m_ViewPass, framebufferDesc));

            framebufferDesc.AttachmentViews.push_back(viewTexture.Scene->Targets[i].Depth->GetAttachmentView(0, 0));
            viewTexture.WorldFramebuffers.push_back(
                RHI::RHIFramebuffer::Create(*m_Device, *m_WorldRenderPass, framebufferDesc));
        }

        // The 2D layer may show a view before a scene is drawn into it, so every color starts
        // cleared and ready to sample.
        m_Device->ExecuteImmediate([&](RHI::RHICommandBuffer& cmd) {
            for (const Iunique<RHI::RHIFramebuffer>& framebuffer : viewTexture.Framebuffers) {
                cmd.BeginRenderPass(*m_ViewPass, *framebuffer, RHI::ClearValue{});
                cmd.EndRenderPass();
            }
        });
        return viewTexture;
    }

    ViewTextureID FrameRenderer::CreateViewTexture(u32 width, u32 height) {
        return m_Views.Emplace(MakeViewTexture(width, height));
    }

    void FrameRenderer::ResizeViewTexture(ViewTextureID id, u32 width, u32 height) {
        ViewTexture* viewTexture = m_Views.Get(id);
        if (!viewTexture) return;
        width = std::max(width, 1u);
        height = std::max(height, 1u);
        if (viewTexture->Width == width && viewTexture->Height == height) return;

        Retire(std::move(*viewTexture));
        *viewTexture = MakeViewTexture(width, height);
    }

    void FrameRenderer::DestroyViewTexture(ViewTextureID id) {
        ViewTexture* viewTexture = m_Views.Get(id);
        if (!viewTexture) return;
        Retire(std::move(*viewTexture));
        m_Views.Remove(id);
    }

    OverlayImageID FrameRenderer::CreateOverlayImage(u32 width, u32 height, const void* pixels) {
        RHI::TextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = RHI::Format::R8G8B8A8_UNORM;
        desc.DebugName = "OverlayImage";
        return m_Images.Emplace(OverlayImage{RHI::RHITexture::Create(*m_Device, desc, pixels)});
    }

    void FrameRenderer::DestroyOverlayImage(OverlayImageID id) {
        OverlayImage* image = m_Images.Get(id);
        if (!image) return;
        Retire(std::move(*image));
        m_Images.Remove(id);
    }

    void FrameRenderer::Retire(ViewTexture view) {
        // A frame being recorded counts as the current one even before it is submitted.
        Retired retired;
        retired.Frame = m_FrameNumber;
        retired.View = std::move(view);
        m_Retired.push_back(std::move(retired));
    }

    void FrameRenderer::Retire(OverlayImage image) {
        Retired retired;
        retired.Frame = m_FrameNumber;
        retired.Image = std::move(image);
        m_Retired.push_back(std::move(retired));
    }

    void FrameRenderer::ReleaseRetired(bool all) {
        u64 framesInFlight = m_Swapchain->GetFramesInFlightCount();
        auto done = [&](const Retired& retired) { return all || retired.Frame + framesInFlight <= m_FrameNumber; };

        for (Retired& retired : m_Retired) {
            if (!done(retired)) continue;
            for (const Iunique<RHI::RHITexture>& color : retired.View.Colors) {
                m_OverlayPass->ForgetTexture(*color);
                m_WorldPass->ForgetTexture(*color);
            }
            if (retired.Image.Texture) {
                m_OverlayPass->ForgetTexture(*retired.Image.Texture);
                m_WorldPass->ForgetTexture(*retired.Image.Texture);
            }
        }
        m_Retired.erase(std::remove_if(m_Retired.begin(), m_Retired.end(), done), m_Retired.end());
    }

} // namespace VKE::Renderer
