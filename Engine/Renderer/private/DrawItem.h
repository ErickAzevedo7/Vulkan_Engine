#pragma once

#include "GpuCache.h"

#include <renderer/RenderTypes.h>

namespace VKE::Renderer {

    // One submesh of one instance, ready to be drawn.
    struct DrawItem {
        const GpuMaterial* Material;
        const GpuMesh* Mesh;
        const Assets::Submesh* Submesh;
        const MeshInstanceDesc* Instance;
    };

} // namespace VKE::Renderer
