#pragma once

#include <renderer/RenderScene.h>

namespace VKE::Scene {

    // What a scene's components feed. Only the scene module sees its definition.
    struct SceneSystems {
        Renderer::RenderScene& Render;
    };

} // namespace VKE::Scene
