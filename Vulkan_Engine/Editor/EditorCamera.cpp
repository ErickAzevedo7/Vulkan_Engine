#include "EditorCamera.h"

#include "Entity.h"
#include "components/Transform.h"
#include "managers/SceneManager.h"

// initialize static variables
glm::vec3 EditorCamera::cameraPos = glm::vec3(0.0f, 1.0f, 3.0f);
glm::vec3 EditorCamera::cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 EditorCamera::cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float EditorCamera::yaw = -90.0f;
float EditorCamera::pitch = 0.0f;
float EditorCamera::lastX = 0.0f;
float EditorCamera::lastY = 0.0f;
bool EditorCamera::isDragging = false;
VkExtent2D EditorCamera::extent;
glm::mat4 EditorCamera::viewMatrix;
glm::mat4 EditorCamera::projMatrix;
ImGuizmo::OPERATION EditorCamera::currentGizmoOperation = ImGuizmo::TRANSLATE;

void EditorCamera::init(VulkanCore* core) {
  engineCore = core;
  int width, height;
  glfwGetFramebufferSize(engineCore->getWindow(), &width, &height);
  EditorCamera::lastX = width / 2.0f;
  EditorCamera::lastY = height / 2.0f;
  EditorCamera::extent = engineCore->getSwapChainExtent();
}

void EditorCamera::setExtent(VkExtent2D newExtent) {
  extent = newExtent;
}

void EditorCamera::updateUniformBuffer(uint32_t currentImage) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  Scene* scene = SceneManager::getActiveScene();

  std::vector<std::unique_ptr<Entity>>* entities = scene->getEntities();

  viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

  projMatrix = glm::perspective(
      glm::radians(45.0f),
      static_cast<float>(extent.width) / static_cast<float>(extent.height),
      0.1f, 1000.0f);

  projMatrix[1][1] *= -1;

  UniformBufferObject ubo{};

  ubo.view = viewMatrix;

  ubo.proj = projMatrix;

  for (const auto& entityPtr : *entities) {
    const Entity& entity = *entityPtr;

    ubo.model = entity.getComponent<Transform>()->getMatrix();

    size_t offset = entity.getID() * VulkanCore::getDynamicAlignment();
    char* base =
        static_cast<char*>(engineCore->getUniformBuffersMapped()[currentImage]);

    memcpy(base + offset, &ubo, sizeof(ubo));
  }

  glm::mat4 view = glm::mat4(glm::mat3(ubo.view));

  Skybox::updateSkyboxUniformBuffer(currentImage, view, ubo.proj);

  GridPlane::updateUniformBuffer(currentImage, glm::mat4(1.0f), ubo.view,
                                 ubo.proj);

  GridPlane::updateGridParamsBuffer(currentImage, cameraPos, 100.0f, 2.0f, 0.5f,
                                    glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
                                    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
}

void EditorCamera::mousePosHandler() {
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImGuiIO& io = ImGui::GetIO();

  float xpos = io.MousePos.x;
  float ypos = io.MousePos.y;

  float xoffset = xpos - lastX;
  float yoffset =
      lastY - ypos;  // reversed since y-coordinates range from bottom to top

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
    ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

    ImVec2 mouseScreenPos = ImGui::GetIO().MousePos;

    ImVec2 mouseInViewport = ImVec2(mouseScreenPos.x - viewportScreenPos.x,
                                    mouseScreenPos.y - viewportScreenPos.y);
    int id;
    if (mouseInViewport.x > 0 && mouseInViewport.y > 0)
      id = mousePick.getEntityIDAt(mouseInViewport.x, mouseInViewport.y);
    else
      id = -1;

    if (id >= 0) {
      SceneUi::selectedEntity = id;
    }
  }

  if (ImGui::IsMouseReleased(1)) {
    isDragging = false;
  }

  if (isDragging) {
    updateCursorLoop();
    mousePosHandler();

    const float cameraSpeed = 2.5f * io.DeltaTime;  // adjust accordingly

    if (ImGui::IsKeyDown(ImGuiKey_W)) {
      cameraPos += cameraSpeed * cameraFront;
    }

    if (ImGui::IsKeyDown(ImGuiKey_S)) {
      cameraPos -= cameraSpeed * cameraFront;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
      cameraPos -=
          glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
      cameraPos +=
          glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
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
  Scene* scene = SceneManager::getActiveScene();
  std::vector<std::unique_ptr<Entity>>* entities = scene->getEntities();
  if (SceneUi::selectedEntity > 0 &&
      SceneUi::selectedEntity <= static_cast<int>(entities->size())) {
    Entity& entity = scene->getEntity(SceneUi::selectedEntity);
    auto* transform = entity.getComponent<Transform>();

    // Get matrices (replace with your actual camera access)
    glm::mat4 view = viewMatrix;
    glm::mat4 proj = projMatrix;
    glm::mat4 model = transform->getMatrix();

    proj[1][1] *= -1;  // Invert Y for ImGuizmo

    // Set up ImGuizmo
    ImGuizmo::SetOrthographic(false);  // Set true if using orthographic camera
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

    ImVec2 viewportScreenSize = ImGui::GetWindowSize();
    ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

    // Set the ImGuizmo rect to match your viewport
    ImGuizmo::SetRect(viewportScreenPos.x, viewportScreenPos.y,
                      viewportScreenSize.x, viewportScreenSize.y);

    // Manipulate
    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             currentGizmoOperation, ImGuizmo::LOCAL,
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
  if (mouse_pos.x < viewport_left) {
    mouse_pos.x = viewport_right;
    moved = true;
  } else if (mouse_pos.x > viewport_right) {
    mouse_pos.x = viewport_left;
    moved = true;
  }

  // Check and wrap vertically
  if (mouse_pos.y < viewport_top) {
    mouse_pos.y = viewport_bottom;
    moved = true;
  } else if (mouse_pos.y > viewport_bottom) {
    mouse_pos.y = viewport_top;
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
