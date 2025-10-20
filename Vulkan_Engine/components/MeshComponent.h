#pragma once
#include "Component.h"
#include "Entity.h"

class MeshComponent : public Component {
 public:
  MeshComponent(Entity* owner,
                const std::string& meshName);
  ~MeshComponent();

  void render(VkCommandBuffer commandBuffer,
              VkPipeline pipeline,
              VkPipelineLayout pipelineLayout,
              VkDescriptorSet descriptorSet) const;
	
  // Accessors
  Mesh* GetMesh() const;
  Entity* GetOwner() const;

 private:
  Entity* owner;
  Mesh* mesh;
  MeshManager* meshManager;
  bool visible;
};
