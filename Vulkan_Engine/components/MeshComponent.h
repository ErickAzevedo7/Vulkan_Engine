#pragma once
#include "Component.h"
#include "Entity.h"
#include "managers/MaterialManager.h"

class MeshComponent : public Component {
 public:
  MeshComponent(Entity* owner,
                const std::string& meshName);
  ~MeshComponent();

  void render(VkCommandBuffer commandBuffer,
              VkPipeline pipeline,
              VkPipelineLayout pipelineLayout, uint32_t imageIndex, int useMousePick) const;
	
  // Accessors
  Mesh* GetMesh() const;
  Entity* GetOwner() const;

 private:
  Entity* owner;
  Mesh* mesh;
  Material* material;
  bool visible;
};
