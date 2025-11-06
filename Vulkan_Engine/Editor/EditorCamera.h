#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>
#include "MousePick.h"
#include "core/vulkancore.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ui/SceneUi.h"

extern float deltaTime;

class EditorCamera  
{  
 public:
  static glm::vec3 cameraPos;
  static glm::vec3 cameraFront;
  static glm::vec3 cameraUp;
  static glm::vec3 direction;
  static float yaw;
  static float pitch;
  static float lastX;
  static float lastY;
  void init(VulkanCore* core);

  void updateUniformBuffer(uint32_t currentImage);
  static void mousePosHandler();
  void inputProcess(MousePick& mousePick);

 private:
  static uint32_t selectedID;
  VulkanCore* engineCore;
  static bool isDragging;
  static void updateCursorLoop();
};
