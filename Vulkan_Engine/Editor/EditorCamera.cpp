#include "EditorCamera.h"

// Project headers - Core
#include "context/ResourceContext.h"
#include "core/vulkancore.h" // UniformBufferObject struct
#include "Entity.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "Scene.h"

// Project headers - Components
#include "components/CameraComponent.h"
#include "components/Transform.h"

// Project headers - Managers
#include "managers/SceneManager.h"

// Project headers - UI and Editor
#include "Editor/MousePick.h"
#include "gizmos/ImGuizmo.h"
#include "gridPlane/GridPlane.h"
#include "skybox/Skybox.h"
#include "ui/InspectorUi.h"

// Third-party - GLM
#include <glm/gtc/matrix_transform.hpp> // lookAt, perspective, radians
#include <glm/gtc/type_ptr.hpp> // value_ptr
#include <glm/gtx/matrix_decompose.hpp> // decompose

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"

// Third-party - Vulkan
#include <vulkan/vulkan_core.h>

// Third-party - GLFW
#include <GLFW/glfw3.h>

// Third-party - ImGui
#include <imgui.h>

// Standard library

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// initialize static variables
glm::vec3 EditorCamera::cameraPos = glm::vec3(0.0f, 1.0f, 3.0f);
glm::vec3 EditorCamera::cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 EditorCamera::cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float EditorCamera::yaw = -90.0f;
float EditorCamera::pitch = 0.0f;
float EditorCamera::lastX = 0.0f;
float EditorCamera::lastY = 0.0f;
bool EditorCamera::isDragging = false;
bool EditorCamera::useGameCameraView = false;
VkExtent2D EditorCamera::editorExtent;
VkExtent2D EditorCamera::gameExtent;
glm::mat4 EditorCamera::viewMatrix;
glm::mat4 EditorCamera::projMatrix;
ImGuizmo::OPERATION EditorCamera::currentGizmoOperation = ImGuizmo::TRANSLATE;
ResourceContext* EditorCamera::resources = nullptr;
InspectorUi* EditorCamera::inspector = nullptr;

void EditorCamera::init(Renderer::VulkanDevice* device, ResourceContext* resources, InspectorUi* inspector) {
	vulkanDevice = device;
	EditorCamera::resources = resources;
	EditorCamera::inspector = inspector;
	int width, height;
	glfwGetFramebufferSize(vulkanDevice->getWindow(), &width, &height);
	EditorCamera::lastX = width / 2.0f;
	EditorCamera::lastY = height / 2.0f;
	EditorCamera::editorExtent = vulkanDevice->getSwapChainExtent();
	EditorCamera::gameExtent = vulkanDevice->getSwapChainExtent();
}

void EditorCamera::setEditorExtent(VkExtent2D newExtent) {
	editorExtent = newExtent;
}

void EditorCamera::setGameExtent(VkExtent2D newExtent) {
	gameExtent = newExtent;
}

void EditorCamera::updateUniformBuffer(uint32_t currentImage,
									   void* uniformBufferMapped,
									   void* editorGlobalBufferMapped,
									   void* gameGlobalBufferMapped,
									   const glm::mat4* lightSpaceMatrices,
									   const glm::vec4& lightPos_farPlane) {
	if (!resources)
		return;
	Scene* scene = resources->getSceneManager().getActiveScene();

	std::vector<std::unique_ptr<Entity>>* entities = scene->getEntities();

	// --- Build Editor GlobalUBO (free-roam camera) ---
	glm::mat4 editorView = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	float editorAspect = (editorExtent.height > 0)
							 ? static_cast<float>(editorExtent.width) / static_cast<float>(editorExtent.height)
							 : 1.0f;
	glm::mat4 editorProj = glm::perspective(glm::radians(45.0f), editorAspect, 0.1f, 1000.0f);
	editorProj[1][1] *= -1;

	// Store editor matrices for gizmo/gridplane use
	viewMatrix = editorView;
	projMatrix = editorProj;

	GlobalUBO editorGlobal{};
	editorGlobal.view = editorView;
	editorGlobal.proj = editorProj;
	editorGlobal.viewPos = cameraPos;
	if (lightSpaceMatrices) {
		for (int i = 0; i < 6; i++) {
			editorGlobal.lightSpaceMatrices[i] = lightSpaceMatrices[i];
		}
		editorGlobal.lightPos_farPlane = lightPos_farPlane;
	}
	memcpy(editorGlobalBufferMapped, &editorGlobal, sizeof(GlobalUBO));

	// --- Build Game GlobalUBO (primary CameraComponent camera) ---
	GlobalUBO gameGlobal{};
	bool foundGameCamera = false;
	for (const auto& entityPtr : *entities) {
		auto* cc = entityPtr->getComponent<CameraComponent>();
		if (cc && cc->isPrimary) {
			auto* tc = entityPtr->getComponent<Transform>();
			if (tc) {
				gameGlobal.view = glm::inverse(tc->getMatrix());
				float gameAspect = (gameExtent.height > 0)
									   ? static_cast<float>(gameExtent.width) / static_cast<float>(gameExtent.height)
									   : 1.0f;
				gameGlobal.proj = cc->getProjectionMatrix(gameAspect);
				gameGlobal.viewPos = glm::vec3(tc->getMatrix()[3]);
				foundGameCamera = true;
				break;
			}
		}
	}
	if (!foundGameCamera) {
		// If no game camera, mirror the editor camera but adjust aspect ratio
		gameGlobal = editorGlobal;

		float gameAspect = (gameExtent.height > 0)
							   ? static_cast<float>(gameExtent.width) / static_cast<float>(gameExtent.height)
							   : 1.0f;
		gameGlobal.proj = glm::perspective(glm::radians(45.0f), gameAspect, 0.1f, 1000.0f);
		gameGlobal.proj[1][1] *= -1;
	}
	if (lightSpaceMatrices) {
		for (int i = 0; i < 6; i++) {
			gameGlobal.lightSpaceMatrices[i] = lightSpaceMatrices[i];
		}
		gameGlobal.lightPos_farPlane = lightPos_farPlane;
	}
	memcpy(gameGlobalBufferMapped, &gameGlobal, sizeof(GlobalUBO));

	// --- Write PerObjectUBO for each entity into the dynamic UBO ---
	for (const auto& entityPtr : *entities) {
		const Entity& entity = *entityPtr;
		auto* t = entity.getComponent<Transform>();
		if (!t)
			continue;

		glm::mat4 model = t->getMatrix();

		PerObjectUBO perObj{};
		perObj.model = model;
		perObj.normal = glm::transpose(glm::inverse(model));

		size_t offset = entity.getID() * vulkanDevice->getDynamicAlignment();
		char* base = static_cast<char*>(uniformBufferMapped);
		memcpy(base + offset, &perObj, sizeof(PerObjectUBO));
	}

	// Skybox and gridplane use editor camera
	glm::mat4 skyView = glm::mat4(glm::mat3(editorView));
	Skybox::updateSkyboxUniformBuffer(currentImage, skyView, editorProj);

	GridPlane::updateUniformBuffer(currentImage, glm::mat4(1.0f), editorView, editorProj);

	GridPlane::updateGridParamsBuffer(currentImage,
									  cameraPos,
									  100.0f,
									  2.0f,
									  0.5f,
									  glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
									  glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
}

void EditorCamera::mousePosHandler() {
	ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	ImGuiIO& io = ImGui::GetIO();

	float xpos = io.MousePos.x;
	float ypos = io.MousePos.y;

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top

	lastX = xpos;
	lastY = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;
	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

void EditorCamera::inputProcess(MousePick& mousePick) {
	if (resources && resources->getSceneManager().getActiveScene()->getState() == SceneState::Play) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(1)) {
		if (!isDragging) {
			isDragging = true;
			ImGuiIO& io = ImGui::GetIO();
			lastX = io.MousePos.x;
			lastY = io.MousePos.y;
		}
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
		// If gizmo is hovered or being used, do not change selection
		if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
			return;
		}

		ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 mouseScreenPos = ImGui::GetIO().MousePos;

		ImVec2 mouseInViewport = ImVec2(mouseScreenPos.x - viewportScreenPos.x, mouseScreenPos.y - viewportScreenPos.y);
		int id;
		if (mouseInViewport.x > 0 && mouseInViewport.y > 0)
			id = mousePick.getEntityIDAt((int)mouseInViewport.x, (int)mouseInViewport.y);
		else
			id = -1;

		if (id >= 0) {
			if (inspector)
				inspector->selectEntity(id);
		}
	}

	if (ImGui::IsMouseReleased(1)) {
		isDragging = false;
	}

	if (isDragging) {
		updateCursorLoop();
		mousePosHandler();

		const float cameraSpeed = 2.5f * io.DeltaTime; // adjust accordingly

		if (ImGui::IsKeyDown(ImGuiKey_W)) {
			cameraPos += cameraSpeed * cameraFront;
		}

		if (ImGui::IsKeyDown(ImGuiKey_S)) {
			cameraPos -= cameraSpeed * cameraFront;
		}
		if (ImGui::IsKeyDown(ImGuiKey_A)) {
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		}
		if (ImGui::IsKeyDown(ImGuiKey_D)) {
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		}
		if (ImGui::IsKeyDown(ImGuiKey_E)) {
			cameraPos += cameraSpeed * cameraUp;
		}
		if (ImGui::IsKeyDown(ImGuiKey_Q)) {
			cameraPos -= cameraSpeed * cameraUp;
		}
	} else {
		if (ImGui::IsKeyPressed(ImGuiKey_W))
			currentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E))
			currentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			currentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_T))
			currentGizmoOperation = ImGuizmo::BOUNDS;
	}
}

void EditorCamera::drawGuizmo() {
	if (!resources)
		return;
	Scene* scene = resources->getSceneManager().getActiveScene();
	if (!scene)
		return;

	auto* entities = scene->getEntities();
	if (!entities || entities->empty())
		return;

	const int selectedId = inspector ? inspector->getSelectedEntityId() : -1;
	if (selectedId <= 0)
		return;

	// Resolve selected entity by its ID, not by index
	Entity* selectedEntity = nullptr;
	for (const auto& ePtr : *entities) {
		if (static_cast<int>(ePtr->getID()) == selectedId) {
			selectedEntity = ePtr.get();
			break;
		}
	}
	if (!selectedEntity)
		return;

	auto* transform = selectedEntity->getComponent<Transform>();
	if (!transform)
		return;

	// Get matrices
	glm::mat4 view = viewMatrix;
	glm::mat4 proj = projMatrix;
	glm::mat4 model = transform->getMatrix();

	proj[1][1] *= -1; // Invert Y for ImGuizmo

	// Set up ImGuizmo
	ImGuizmo::SetOrthographic(false); // Set true if using orthographic camera
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	ImVec2 viewportScreenSize = ImGui::GetWindowSize();
	ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

	// Set the ImGuizmo rect to match your viewport
	ImGuizmo::SetRect(viewportScreenPos.x, viewportScreenPos.y, viewportScreenSize.x, viewportScreenSize.y);

	// Manipulate
	if (ImGuizmo::Manipulate(glm::value_ptr(view),
							 glm::value_ptr(proj),
							 currentGizmoOperation,
							 ImGuizmo::LOCAL,
							 glm::value_ptr(model))) {
		// If the gizmo is used, decompose the matrix back to
		// position/rotation/scale
		glm::vec3 translation, scale, skew;
		glm::vec4 perspective;
		glm::quat rotation;
		glm::decompose(model, scale, rotation, translation, skew, perspective);

		transform->position = translation;
		transform->rotation = rotation;
		transform->scale = scale;

		if (resources) {
			if (Scene* scene = resources->getSceneManager().getActiveScene()) {
				scene->markDirty();
			}
		}
	}
}

glm::mat4 EditorCamera::getViewMatrix() {
	return viewMatrix;
}

glm::mat4 EditorCamera::getProjMatrix() {
	return projMatrix;
}

void EditorCamera::updateCursorLoop() {
	ImGuiIO& io = ImGui::GetIO();
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	// Get the current mouse position
	ImVec2 mouse_pos = io.MousePos;

	// Get the viewport boundaries
	float viewport_left = viewport->Pos.x;
	float viewport_right = viewport->Pos.x + viewport->Size.x;
	float viewport_top = viewport->Pos.y;
	float viewport_bottom = viewport->Pos.y + viewport->Size.y;

	bool moved = false;

	// Check and wrap horizontally
	if (mouse_pos.x <= viewport_left) {
		mouse_pos.x = viewport_right - 2.0f;
		moved = true;
	} else if (mouse_pos.x >= viewport_right - 1.0f) {
		mouse_pos.x = viewport_left + 2.0f;
		moved = true;
	}

	// Check and wrap vertically
	if (mouse_pos.y <= viewport_top) {
		mouse_pos.y = viewport_bottom - 2.0f;
		moved = true;
	} else if (mouse_pos.y >= viewport_bottom - 1.0f) {
		mouse_pos.y = viewport_top + 2.0f;
		moved = true;
	}

	// Update ImGui's internal mouse position
	if (moved) {
		io.MousePos = mouse_pos;
		// Also update the native cursor position via the backend
		io.WantSetMousePos = true;
		lastX = mouse_pos.x;
		lastY = mouse_pos.y;
	}
}
