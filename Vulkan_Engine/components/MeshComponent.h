#pragma once

#include <cstdint>
#include <string>

#include "Component.h"
#include "vulkan/vulkan_core.h"

// Forward declarations
class Entity;
namespace Renderer {
class GraphicsResourceBinder;
}
struct Mesh;
struct Material;

class MeshManager; // Forward declaration

class MeshComponent : public Component {
public:
	MeshComponent(Entity* owner, const std::string& meshName, MeshManager& meshManager);
	~MeshComponent();

	void render(VkCommandBuffer commandBuffer,
				VkPipeline pipeline,
				VkPipelineLayout pipelineLayout,
				uint32_t currentFrame,
				uint64_t dynamicAlignment,
				int useMousePick,
				MeshManager& meshManager,
				Renderer::GraphicsResourceBinder& binder) const;

	// Accessors
	Mesh* GetMesh() const;
	Entity* GetOwner() const;

	// Accessors for the material so the inspector can change it
	Material* GetMaterial() const {
		return material;
	}
	void SetMaterial(Material* m) {
		if (m)
			material = m;
	}

private:
	Entity* owner;
	Mesh* mesh;
	Material* material;
	bool visible;
};
