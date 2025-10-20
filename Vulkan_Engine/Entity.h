#pragma once
#include <vulkan/vulkan.h>
#include <type_traits>
#include <vector>
#include "components/Component.h"
#include "core/vulkancore.h"

class Entity {
 public:
  Entity(std::string name);
  Entity(std::vector<Vertex> vertices,
         std::vector<uint32_t> indices,
         VulkanCore* engineCore);
  ~Entity();

  std::string getName() const;

  template <typename T>
  T* getComponent() const {
    static_assert(std::is_base_of<Component, T>::value,
                  "T must inherit from Component");
    for (const auto& component : components) {
      if (T* casted = dynamic_cast<T*>(component)) {
        return casted;
      }
    }
    return nullptr;
  }

  void addComponent(Component* component);

 private:
  std::vector<Component*> components;
  std::string name;
};
