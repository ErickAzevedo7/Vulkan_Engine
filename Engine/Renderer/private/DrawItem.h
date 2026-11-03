#pragma once

#include "GpuCache.h"

#include <math/AABB.h>
#include <renderer/RenderTypes.h>

namespace VKE::Renderer {

// One submesh of one instance, ready to be drawn.
struct DrawItem {
	const GpuMaterial *Material = nullptr;
	const GpuMesh *Mesh = nullptr;
	const Assets::Submesh *Submesh = nullptr;
	const MeshInstanceDesc *Instance = nullptr;
	// The instance's whole mesh, in world space.
	AABB WorldBounds;
};

} // namespace VKE::Renderer
