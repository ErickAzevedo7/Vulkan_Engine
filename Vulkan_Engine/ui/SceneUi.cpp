#include "SceneUi.h"

int SceneUi::selectedEntity = -1;

void SceneUi::render() {
  auto* scene = SceneManager::getActiveScene();
  const auto& entities = scene->getEntities();

  ImGui::Begin("scene");
  if (scene) {
    for (size_t i = 0; i < scene->getEntityCount(); ++i) {
      Entity& entity = scene->getEntity(i);

      bool isSelected = (selectedEntity == static_cast<int>(i));

      std::string label = entity.getName().empty()
                              ? ("Entity " + std::to_string(i))
                              : entity.getName();

      // Selectable entity
      if (ImGui::Selectable(label.c_str(), selectedEntity == static_cast<int>(i))) {
        selectedEntity = static_cast<int>(i);
      }

      // Right-click context menu for each entity
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Remove")) {
          scene->removeEntity(i);
          if (selectedEntity == static_cast<int>(i))
            selectedEntity = -1;
          ImGui::EndPopup();
          break;  // Entities list changed, break out of loop
        }
        ImGui::EndPopup();
      }
    }

    // If an entity is selected, show editing options
    if (selectedEntity >= 0 && selectedEntity < static_cast<int>(scene->getEntityCount())) {
      ImGui::Separator();
      Entity& entity = scene->getEntity(selectedEntity);
      char nameBuffer[128];
      strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
      nameBuffer[sizeof(nameBuffer) - 1] = 0;
      // Add more component editing here as needed
    }
  } else {
    ImGui::Text("No active scene.");
  }
  ImGui::End();

  // --- Inspector Window ---
  ImGui::Begin("Inspector");
  if (selectedEntity >= 0 && selectedEntity < static_cast<int>(entities->size())) {
    Entity& entity = scene->getEntity(selectedEntity);
    char nameBuffer[256];
    strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
    nameBuffer[sizeof(nameBuffer) - 1] = 0;
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
      entity.setName(nameBuffer);
    }

  	// --- TransformComponent Editing ---
    //if (entity.hasComponent<TransformComponent>()) {
      auto* transform = entity.getComponent<Transform>();
      ImGui::Separator();
      ImGui::Text("Transform");

      // Position
      ImGui::DragFloat3("Position", &transform->position.x, 0.1f);

      // Rotation
      glm::vec3 euler = glm::degrees(glm::eulerAngles(transform->rotation));
      if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f)) {
        // Convert back to quaternion (expects radians)
        transform->rotation = glm::quat(glm::radians(euler));
      }

      // Scale
      ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
    //}
  } else {
    ImGui::Text("No entity selected.");
  }
  ImGui::End();
}
