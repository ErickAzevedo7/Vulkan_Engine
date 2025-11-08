#include "EditorCamera.h"

#include "Entity.h"
#include "components/Transform.h"
#include "managers/SceneManager.h"

// initialize static variables
glm::vec3 EditorCamera::cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 EditorCamera::cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 EditorCamera::cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float EditorCamera::yaw = -90.0f;
float EditorCamera::pitch = 0.0f;
float EditorCamera::lastX = 0.0f;
float EditorCamera::lastY = 0.0f;
bool EditorCamera::isDragging = false;

void EditorCamera::init(VulkanCore* core) {
  engineCore = core;
  int width, height;
  glfwGetFramebufferSize(engineCore->getWindow(), &width, &height);
  EditorCamera::lastX = width / 2.0f;
  EditorCamera::lastY = height / 2.0f;
}

void EditorCamera::updateUniformBuffer(uint32_t currentImage) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  Scene* scene = SceneManager::getActiveScene();

  std::vector<std::unique_ptr<Entity>>* entities = scene->getEntities();

	UniformBufferObject ubo{};

	ubo.view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

  ubo.proj =
      glm::perspective(glm::radians(45.0f),
                       engineCore->getSwapChainExtent().width /
                           (float)engineCore->getSwapChainExtent().height,
                       0.1f, 10.0f);

  ubo.proj[1][1] *= -1;

  for (const auto& entityPtr : *entities) {
    const Entity& entity = *entityPtr;

    ubo.model = entity.getComponent<Transform>()->getMatrix();

    size_t offset = entity.getID() * VulkanCore::getDynamicAlignment();
    char* base =
        static_cast<char*>(engineCore->getUniformBuffersMapped()[currentImage]);

    memcpy(base + offset, &ubo, sizeof(ubo));
  }

  glm::mat4 view = glm::mat4(
      glm::mat3(ubo.view));

  Skybox::updateSkyboxUniformBuffer(currentImage, view, ubo.proj);
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
  	
    int id = mousePick.getEntityIDAt(mouseInViewport.x, mouseInViewport.y);

    if (id >= 0){
      SceneUi::selectedEntity = id;
    }
  }

  if (ImGui::IsMouseReleased(1)) {
    isDragging = false;
  }

  if (isDragging) {
    updateCursorLoop();
    mousePosHandler();
  }

  const float cameraSpeed = 2.5f * io.DeltaTime;  // adjust accordingly

  if (ImGui::IsKeyDown(ImGuiKey_W))
    cameraPos += cameraSpeed * cameraFront;
  if (ImGui::IsKeyDown(ImGuiKey_S))
    cameraPos -= cameraSpeed * cameraFront;
  if (ImGui::IsKeyDown(ImGuiKey_A))
    cameraPos -=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  if (ImGui::IsKeyDown(ImGuiKey_D))
    cameraPos +=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
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
