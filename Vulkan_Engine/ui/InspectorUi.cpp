#include "InspectorUi.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

InspectorSelection InspectorUi::selection{};

void InspectorUi::selectEntity(int entityId) {
  selection.type = InspectorSelectionType::Entity;
  selection.entityId = entityId;
  // Clear any asset selection state when an entity is picked
	selection.assetPath.clear();

  // Update per-entity selection flags so outline/rendering can use isSelected
  Scene* scene = SceneManager::getActiveScene();
  if (!scene) return;

  auto* entities = scene->getEntities();
  if (!entities) return;

  for (const auto& ePtr : *entities) {
    Entity& e = *ePtr;
    e.isSelected = (static_cast<int>(e.getID()) == entityId);
  }
}

void InspectorUi::selectAsset(const std::string& assetPath) {
  selection.type = InspectorSelectionType::Asset;
  selection.entityId = 0;
  selection.assetPath = assetPath;

  // Clear entity selection flags when switching to an asset selection
  Scene* scene = SceneManager::getActiveScene();
  if (!scene) return;

  auto* entities = scene->getEntities();
  if (!entities) return;

  for (const auto& ePtr : *entities) {
    ePtr->isSelected = false;
  }
}

void InspectorUi::clearSelection() {
  selection.type = InspectorSelectionType::None;
  selection.entityId = 0;
  selection.assetPath.clear();

   // Clear all per-entity selection flags
   Scene* scene = SceneManager::getActiveScene();
   if (!scene) return;

   auto* entities = scene->getEntities();
   if (!entities) return;

   for (const auto& ePtr : *entities) {
     ePtr->isSelected = false;
   }
}

int InspectorUi::getSelectedEntityId() {
  if (selection.type == InspectorSelectionType::Entity) {
    return selection.entityId;
  }
  return -1;
}

void InspectorUi::render() {
  Scene* scene = SceneManager::getActiveScene();

  ImGui::Begin("Inspector");
  if (selection.type == InspectorSelectionType::Entity && scene &&
      selection.entityId > 0 &&
      selection.entityId <= static_cast<int>(scene->getEntityCount())) {
    Entity& entity =
        scene->getEntity(static_cast<size_t>(selection.entityId));
    char nameBuffer[256];
    strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
    nameBuffer[sizeof(nameBuffer) - 1] = 0;
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
      entity.setName(nameBuffer);
    }

    auto* transform = entity.getComponent<Transform>();
    ImGui::Separator();
    ImGui::Text("Transform");

    ImGui::DragFloat3("Position", &transform->position.x, 0.1f);

    glm::vec3 euler = glm::degrees(glm::eulerAngles(transform->rotation));
    if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f)) {
      transform->rotation = glm::quat(glm::radians(euler));
    }

    ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
  } else {
    ImGui::Text("No entity selected.");
  }

  ImGui::Separator();
  ImGui::Text("Asset");
  if (selection.type == InspectorSelectionType::Asset &&
	      !selection.assetPath.empty()) {
		ImGui::TextWrapped("Path: %s", selection.assetPath.c_str());

		const std::string& fullPath = selection.assetPath;
    std::filesystem::path p(fullPath);

    std::string fileName = p.filename().string();
    std::string extension = p.has_extension() ? p.extension().string() : "";

    ImGui::Text("Name: %s", fileName.c_str());
    ImGui::Text("Type: %s", extension.empty() ? "Folder / No extension"
                                              : extension.c_str());
  } else {
    ImGui::Text("No asset selected.");
  }

  ImGui::End();
}
