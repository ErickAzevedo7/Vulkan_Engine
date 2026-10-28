#include "renderer/Renderer.h"
#include "FrameRenderer.h"

#include <core/Log.h>

namespace VKE::Renderer {

    Renderer::Renderer(Platform::IWindow& window, Assets::AssetManager& assets) : m_Window(window), m_Assets(assets) {
    }

    Renderer::~Renderer() = default;

    void Renderer::OnInitialize() {
        m_Frame = MakeIunique<FrameRenderer>(m_Window, m_Assets);
        m_Frame->SetExposure(m_Exposure);
        m_Frame->SetShadowsEnabled(m_ShadowsEnabled);
        VKE_CORE_INFO("Renderer initialized");
    }

    void Renderer::OnShutdown() {
        // GPU resources go before the window they present to.
        m_Frame.reset();
        VKE_CORE_INFO("Renderer shutdown");
    }

    bool Renderer::BeginFrame() {
        return m_Frame && m_Frame->BeginFrame();
    }

    void Renderer::DrawScene(const RenderScene& scene, const RenderView& view, ViewTextureID target) {
        if (m_Frame) m_Frame->DrawScene(scene, view, target);
    }

    void Renderer::DrawOverlay(const OverlayDrawList& list) {
        if (m_Frame) m_Frame->DrawOverlay(list);
    }

    void Renderer::EndFrame() {
        if (m_Frame) m_Frame->EndFrame();
    }

    void Renderer::Render(const RenderScene& scene, const RenderView& view) {
        if (BeginFrame()) {
            DrawScene(scene, view);
            EndFrame();
        }
    }

    ViewTextureID Renderer::CreateViewTexture(u32 width, u32 height) {
        return m_Frame ? m_Frame->CreateViewTexture(width, height) : ViewTextureID{};
    }

    void Renderer::ResizeViewTexture(ViewTextureID id, u32 width, u32 height) {
        if (m_Frame) m_Frame->ResizeViewTexture(id, width, height);
    }

    void Renderer::DestroyViewTexture(ViewTextureID id) {
        if (m_Frame) m_Frame->DestroyViewTexture(id);
    }

    OverlayImageID Renderer::CreateOverlayImage(u32 width, u32 height, const void* pixels) {
        return m_Frame ? m_Frame->CreateOverlayImage(width, height, pixels) : OverlayImageID{};
    }

    void Renderer::DestroyOverlayImage(OverlayImageID id) {
        if (m_Frame) m_Frame->DestroyOverlayImage(id);
    }

    void Renderer::SetExposure(f32 exposure) {
        m_Exposure = exposure;
        if (m_Frame) m_Frame->SetExposure(exposure);
    }

    void Renderer::SetShadowsEnabled(bool enabled) {
        m_ShadowsEnabled = enabled;
        if (m_Frame) m_Frame->SetShadowsEnabled(enabled);
    }

} // namespace VKE::Renderer
