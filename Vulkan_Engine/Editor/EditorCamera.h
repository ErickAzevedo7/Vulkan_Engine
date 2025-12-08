#pragma once
#include <vulkan/vulkan.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "MousePick.h"
#include "core/vulkancore.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ui/SceneUi.h"
#include <ImGuizmo.h>

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

  static void setExtent(VkExtent2D newExtent);

  void updateUniformBuffer(uint32_t currentImage);
  static void mousePosHandler();
  void inputProcess(MousePick& mousePick);
  static void drawGuizmo();
  static glm::mat4 getViewMatrix();
  static glm::mat4 getProjMatrix();

 private:
  static uint32_t selectedID;
  VulkanCore* engineCore;
  static bool isDragging;
  static void updateCursorLoop();
  static VkExtent2D extent;
  static glm::mat4 viewMatrix;
  static glm::mat4 projMatrix;
  static ImGuizmo::OPERATION currentGizmoOperation;
};
