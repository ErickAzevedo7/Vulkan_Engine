#include "renderer/Renderer.h"
#include "ForwardPath.h"

#include <core/Log.h>

namespace VKE::Renderer {

    Renderer::Renderer(Platform::IWindow& window, Assets::AssetManager& assets) : m_Window(window), m_Assets(assets) {
    }

    Renderer::~Renderer() = default;

    void Renderer::OnInitialize() {
        m_ForwardPath = MakeIunique<ForwardPath>(m_Window, m_Assets);
        VKE_CORE_INFO("Renderer initialized");
    }

    void Renderer::OnShutdown() {
        // GPU resources go before the window they present to.
        if (m_ForwardPath) {
            m_ForwardPath->WaitIdle();
            m_ForwardPath.reset();
        }
        VKE_CORE_INFO("Renderer shutdown");
    }

    void Renderer::Render(const RenderScene& scene, const RenderView& view) {
        if (m_ForwardPath) {
            m_ForwardPath->Render(scene, view);
        }
    }

} // namespace VKE::Renderer
