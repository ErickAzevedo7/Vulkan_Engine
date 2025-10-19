#include "SceneUi.h"

#include "imgui.h"
#include "scene/SceneManager.h"

int SceneUi::selectedEntity = -1;

void SceneUi::render() {
  ImGui::Begin("scene");
  if (auto* scene = SceneManager::getActiveScene()) {
    for (size_t i = 0; i < scene->getEntityCount(); ++i) {
      Entity& entity = scene->getEntity(i);
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
          if (selectedEntity == (int)i)
            selectedEntity = -1;
          ImGui::EndPopup();
          break;  // Entities list changed, break out of loop
        }
        ImGui::EndPopup();
      }
    }

    // If an entity is selected, show editing options
    if (selectedEntity >= 0 && selectedEntity < (int)scene->getEntityCount()) {
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
}
