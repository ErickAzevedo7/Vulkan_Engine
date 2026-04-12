#include "EditorCamera.h"

// Project headers - Core
#include "context/ResourceContext.h"
#include "core/vulkancore.h" // UniformBufferObject struct
#include "Entity.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "Scene.h"

// Project headers - Components
#include "components/CameraComponent.h"
#include "components/ColliderComponent.h"
#include "components/MeshComponent.h"
#include "components/StaticMeshColliderComponent.h"
#include "components/Transform.h"

// Project headers - Managers
#include "managers/SceneManager.h"
#include "managers/MeshManager.h"

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
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
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
ImGuizmo::MODE EditorCamera::currentGizmoMode = ImGuizmo::LOCAL;
bool EditorCamera::showColliders = false;
ResourceContext* EditorCamera::resources = nullptr;
InspectorUi* EditorCamera::inspector = nullptr;

namespace {

bool projectClipToScreen(const glm::vec4& clip,
					 const ImVec2& viewportPos,
					 const ImVec2& viewportSize,
					 ImVec2& out) {
	if (clip.w <= 0.0001f) {
		return false;
	}
	const glm::vec3 ndc = glm::vec3(clip) / clip.w;
	out.x = viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
	out.y = viewportPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y;
	return true;
}

void drawLine3D(ImDrawList* drawList,
				const glm::vec3& a,
				const glm::vec3& b,
				const glm::mat4& viewProj,
				const ImVec2& viewportPos,
				const ImVec2& viewportSize,
				ImU32 color) {
	constexpr float kNearW = 0.0001f;
	glm::vec4 clipA = viewProj * glm::vec4(a, 1.0f);
	glm::vec4 clipB = viewProj * glm::vec4(b, 1.0f);

	if (clipA.w <= kNearW && clipB.w <= kNearW) {
		return;
	}

	if (clipA.w <= kNearW || clipB.w <= kNearW) {
		const float wa = clipA.w;
		const float wb = clipB.w;
		const float denom = wb - wa;
		if (std::abs(denom) <= 1e-8f) {
			return;
		}
		float t = (kNearW - wa) / denom;
		t = glm::clamp(t, 0.0f, 1.0f);
		const glm::vec4 clipped = clipA + (clipB - clipA) * t;
		if (clipA.w <= kNearW) {
			clipA = clipped;
		} else {
			clipB = clipped;
		}
	}

	ImVec2 a2;
	ImVec2 b2;
	if (!projectClipToScreen(clipA, viewportPos, viewportSize, a2)) {
		return;
	}
	if (!projectClipToScreen(clipB, viewportPos, viewportSize, b2)) {
		return;
	}
	drawList->AddLine(a2, b2, color, 1.2f);
}

void drawAabbWireframe(ImDrawList* drawList,
					   const glm::vec3& worldMin,
					   const glm::vec3& worldMax,
					   const glm::mat4& viewProj,
					   const ImVec2& viewportPos,
					   const ImVec2& viewportSize,
					   ImU32 color) {
	const glm::vec3 corners[8] = {
		{worldMin.x, worldMin.y, worldMin.z},
		{worldMax.x, worldMin.y, worldMin.z},
		{worldMax.x, worldMax.y, worldMin.z},
		{worldMin.x, worldMax.y, worldMin.z},
		{worldMin.x, worldMin.y, worldMax.z},
		{worldMax.x, worldMin.y, worldMax.z},
		{worldMax.x, worldMax.y, worldMax.z},
		{worldMin.x, worldMax.y, worldMax.z}
	};

	const int edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	for (const auto& edge : edges) {
		drawLine3D(drawList,
				   corners[edge[0]],
				   corners[edge[1]],
				   viewProj,
				   viewportPos,
				   viewportSize,
				   color);
	}
}

} // namespace

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

	// Get matrices
	glm::mat4 view = viewMatrix;
	glm::mat4 proj = projMatrix;

	proj[1][1] *= -1; // Invert Y for ImGuizmo

	ImVec2 viewportScreenSize = ImGui::GetContentRegionAvail();
	ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();
	if (viewportScreenSize.x <= 1.0f || viewportScreenSize.y <= 1.0f) {
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const glm::mat4 viewProj = proj * view;

	// Draw all colliders in scene if toggle is on
	if (showColliders) {
		for (const auto& ePtr : *entities) {
			if (!ePtr) continue;
			Entity* entity = ePtr.get();
			auto* transform = entity->getComponent<Transform>();
			if (!transform) continue;

			// ColliderComponent
			if (auto* collider = entity->getComponent<ColliderComponent>()) {
				if (collider->enabled) {
					const glm::vec3 worldCenter = transform->getWorldPosition() + collider->center;
					const glm::vec3 absScale = glm::abs(transform->getWorldScale());
					const glm::vec3 halfExtents = glm::max(collider->size * absScale * 0.5f, glm::vec3(0.0001f));
					const glm::vec3 worldMin = worldCenter - halfExtents;
					const glm::vec3 worldMax = worldCenter + halfExtents;
					const ImU32 color = collider->isTrigger ? IM_COL32(255, 179, 0, 255) : IM_COL32(96, 255, 128, 255);
					drawAabbWireframe(drawList, worldMin, worldMax, viewProj, viewportScreenPos, viewportScreenSize, color);
				}
			}

			// StaticMeshColliderComponent
			if (auto* staticMeshCollider = entity->getComponent<StaticMeshColliderComponent>()) {
				if (staticMeshCollider->enabled) {
					const ImU32 color = staticMeshCollider->isTrigger ? IM_COL32(255, 140, 64, 255) : IM_COL32(64, 220, 255, 255);
					auto* meshComp = entity->getComponent<MeshComponent>();
					if (staticMeshCollider->useAttachedMeshBounds && meshComp) {
						if (Mesh* mesh = meshComp->GetMesh()) {
							if (!mesh->collisionVertices.empty() && mesh->collisionIndices.size() >= 3) {
								const glm::vec3 worldPos = transform->getWorldPosition();
								const glm::quat worldRot = transform->getWorldRotation();
								const glm::vec3 worldScale = transform->getWorldScale();
								const glm::vec3 localCenter = staticMeshCollider->localCenter;
								const glm::vec3 localSize = glm::max(staticMeshCollider->localSize, glm::vec3(0.0001f));
								size_t segmentBudget = 12000;

								auto toWorld = [&](const glm::vec3& localVertex) {
									const glm::vec3 shapedLocal = localCenter + (localVertex * localSize);
									const glm::vec3 scaled = shapedLocal * worldScale;
									return worldPos + (worldRot * scaled);
								};

								for (size_t i = 0; i + 2 < mesh->collisionIndices.size() && segmentBudget >= 3; i += 3) {
									const uint32_t i0 = mesh->collisionIndices[i];
									const uint32_t i1 = mesh->collisionIndices[i + 1];
									const uint32_t i2 = mesh->collisionIndices[i + 2];
									if (i0 >= mesh->collisionVertices.size() ||
										i1 >= mesh->collisionVertices.size() ||
										i2 >= mesh->collisionVertices.size()) {
										continue;
									}

									const glm::vec3 w0 = toWorld(mesh->collisionVertices[i0]);
									const glm::vec3 w1 = toWorld(mesh->collisionVertices[i1]);
									const glm::vec3 w2 = toWorld(mesh->collisionVertices[i2]);

									drawLine3D(drawList, w0, w1, viewProj, viewportScreenPos, viewportScreenSize, color);
									drawLine3D(drawList, w1, w2, viewProj, viewportScreenPos, viewportScreenSize, color);
									drawLine3D(drawList, w2, w0, viewProj, viewportScreenPos, viewportScreenSize, color);
									segmentBudget -= 3;
								}
							}
						}
					}

					if (!meshComp || !staticMeshCollider->useAttachedMeshBounds) {
						const glm::vec3 worldPos = transform->getWorldPosition();
						const glm::vec3 worldCenter = worldPos + staticMeshCollider->localCenter;
						const glm::vec3 absScale = glm::abs(transform->getWorldScale());
						const glm::vec3 halfExtents = glm::max(staticMeshCollider->localSize * absScale * 0.5f, glm::vec3(0.0001f));
						const glm::vec3 worldMin = worldCenter - halfExtents;
						const glm::vec3 worldMax = worldCenter + halfExtents;
						drawAabbWireframe(drawList, worldMin, worldMax, viewProj, viewportScreenPos, viewportScreenSize, color);
					}
				}
			}
		}
	}

	// Original behavior: only draw selected entity collider
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

	glm::mat4 model = transform->getMatrix();

	if (auto* collider = selectedEntity->getComponent<ColliderComponent>()) {
		if (collider->enabled) {
			const glm::vec3 worldCenter = transform->getWorldPosition() + collider->center;
			const glm::vec3 absScale = glm::abs(transform->getWorldScale());
			const glm::vec3 halfExtents = glm::max(collider->size * absScale * 0.5f, glm::vec3(0.0001f));
			const glm::vec3 worldMin = worldCenter - halfExtents;
			const glm::vec3 worldMax = worldCenter + halfExtents;
			const ImU32 color = collider->isTrigger ? IM_COL32(255, 179, 0, 255) : IM_COL32(96, 255, 128, 255);
			drawAabbWireframe(drawList, worldMin, worldMax, viewProj, viewportScreenPos, viewportScreenSize, color);
		}
	}

	if (auto* staticMeshCollider = selectedEntity->getComponent<StaticMeshColliderComponent>()) {
		if (staticMeshCollider->enabled) {
			const ImU32 color = staticMeshCollider->isTrigger ? IM_COL32(255, 140, 64, 255) : IM_COL32(64, 220, 255, 255);
			auto* meshComp = selectedEntity->getComponent<MeshComponent>();
			bool drewTriangles = false;
			if (staticMeshCollider->useAttachedMeshBounds && meshComp) {
				if (Mesh* mesh = meshComp->GetMesh()) {
					if (!mesh->collisionVertices.empty() && mesh->collisionIndices.size() >= 3) {
						const glm::vec3 worldPos = transform->getWorldPosition();
						const glm::quat worldRot = transform->getWorldRotation();
						const glm::vec3 worldScale = transform->getWorldScale();
						const glm::vec3 localCenter = staticMeshCollider->localCenter;
						const glm::vec3 localSize = glm::max(staticMeshCollider->localSize, glm::vec3(0.0001f));
						size_t segmentBudget = 12000;

						auto toWorld = [&](const glm::vec3& localVertex) {
							const glm::vec3 shapedLocal = localCenter + (localVertex * localSize);
							const glm::vec3 scaled = shapedLocal * worldScale;
							return worldPos + (worldRot * scaled);
						};

						for (size_t i = 0; i + 2 < mesh->collisionIndices.size() && segmentBudget >= 3; i += 3) {
							const uint32_t i0 = mesh->collisionIndices[i];
							const uint32_t i1 = mesh->collisionIndices[i + 1];
							const uint32_t i2 = mesh->collisionIndices[i + 2];
							if (i0 >= mesh->collisionVertices.size() ||
								i1 >= mesh->collisionVertices.size() ||
								i2 >= mesh->collisionVertices.size()) {
								continue;
							}

							const glm::vec3 w0 = toWorld(mesh->collisionVertices[i0]);
							const glm::vec3 w1 = toWorld(mesh->collisionVertices[i1]);
							const glm::vec3 w2 = toWorld(mesh->collisionVertices[i2]);

							drawLine3D(drawList, w0, w1, viewProj, viewportScreenPos, viewportScreenSize, color);
							drawLine3D(drawList, w1, w2, viewProj, viewportScreenPos, viewportScreenSize, color);
							drawLine3D(drawList, w2, w0, viewProj, viewportScreenPos, viewportScreenSize, color);
							segmentBudget -= 3;
							drewTriangles = true;
						}
					}
				}
			}

			if (!drewTriangles) {
				const glm::vec3 localCenter = staticMeshCollider->localCenter;
				const glm::vec3 localSize = glm::max(staticMeshCollider->localSize, glm::vec3(0.0001f));
				const glm::vec3 worldCenter = transform->getWorldPosition() + localCenter;
				const glm::vec3 absScale = glm::abs(transform->getWorldScale());
				const glm::vec3 halfExtents = glm::max(localSize * absScale * 0.5f, glm::vec3(0.0001f));
				drawAabbWireframe(
					drawList,
					worldCenter - halfExtents,
					worldCenter + halfExtents,
					viewProj,
					viewportScreenPos,
					viewportScreenSize,
					color);
			}
		}
	}

	// Set the ImGuizmo rect to match your viewport
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	ImGuizmo::SetRect(viewportScreenPos.x, viewportScreenPos.y, viewportScreenSize.x, viewportScreenSize.y);

	// Manipulate
	if (ImGuizmo::Manipulate(glm::value_ptr(view),
							 glm::value_ptr(proj),
							 currentGizmoOperation,
							 currentGizmoMode,
							 glm::value_ptr(model))) {
		transform->setFromWorldMatrix(model);

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

ImGuizmo::MODE EditorCamera::getGizmoMode() {
	return currentGizmoMode;
}

void EditorCamera::setGizmoMode(ImGuizmo::MODE mode) {
	currentGizmoMode = mode;
}

bool EditorCamera::getShowColliders() {
	return showColliders;
}

void EditorCamera::setShowColliders(bool show) {
	showColliders = show;
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
