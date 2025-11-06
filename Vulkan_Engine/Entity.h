#pragma once
#include <vulkan/vulkan.h>
#include <type_traits>
#include <vector>
#include "components/Component.h"
#include "core/vulkancore.h"

class Entity {
 public:
  bool isSelected = false;

  Entity(std::string name);

  ~Entity();

  std::string getName() const;

  uint32_t getID() const { return id; }

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

  void setName(char* str);

private:
  static std::atomic<uint32_t> nextID;
  std::vector<Component*> components;
  std::string name;
  uint32_t id;
};
