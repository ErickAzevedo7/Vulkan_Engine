#pragma once

#include "Component.h"
#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <string>

// Forward declarations
class Entity;
struct Mesh;
struct Material;

class MeshComponent : public Component {
public:
	MeshComponent(Entity* owner, const std::string& meshName);
	~MeshComponent();

	void render(VkCommandBuffer commandBuffer,
				VkPipeline pipeline,
				VkPipelineLayout pipelineLayout,
				uint32_t imageIndex,
				int useMousePick) const;

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
