#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <imgui.h>

#include "gizmos/ImGuizmo.h"
#include "ui/InspectorUi.h" // Needed for InspectorUi type
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"

// Forward declarations
class MousePick;
class ResourceContext;

namespace Renderer {
class VulkanDevice;
}

extern double deltaTime;

class EditorCamera {
public:
	static glm::vec3 cameraPos;
	static glm::vec3 cameraFront;
	static glm::vec3 cameraUp;
	static glm::vec3 direction;
	static float yaw;
	static float pitch;
	static float lastX;
	static float lastY;
	static bool useGameCameraView;
	void init(Renderer::VulkanDevice* device, ResourceContext* resources, InspectorUi* inspector);
	static void setEditorExtent(VkExtent2D newExtent);
	static void setGameExtent(VkExtent2D newExtent);

	void updateUniformBuffer(uint32_t currentImage,
							 void* uniformBufferMapped,
							 void* editorGlobalBufferMapped,
							 void* gameGlobalBufferMapped,
							 const glm::mat4* lightSpaceMatrices = nullptr,
							 const glm::vec4& lightPos_farPlane = glm::vec4(0.0f));
	static void mousePosHandler();
	void inputProcess(MousePick& mousePick);
	static void drawGuizmo();
	static glm::mat4 getViewMatrix();
	static glm::mat4 getProjMatrix();

private:
	static uint32_t selectedID;
	Renderer::VulkanDevice* vulkanDevice;
	static ResourceContext* resources;
	static InspectorUi* inspector;
	static bool isDragging;
	static void updateCursorLoop();
	static VkExtent2D editorExtent;
	static VkExtent2D gameExtent;
	static glm::mat4 viewMatrix;
	static glm::mat4 projMatrix;
	static ImGuizmo::OPERATION currentGizmoOperation;
};
