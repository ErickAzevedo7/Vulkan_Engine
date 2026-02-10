#include "SceneUi.h"

#include <cstddef>
#include <string>

#include "context/ResourceContext.h"
#include "Entity.h"
#include "imgui.h"
#include "managers/SceneManager.h"
#include "Scene.h"
#include "ui/InspectorUi.h"

SceneUi::SceneUi(ResourceContext& resources, InspectorUi& inspector) : resources(resources), inspector(inspector) {
	selectedEntity = -1;
}

void SceneUi::render() {
	Scene* scene = resources.getSceneManager().getActiveScene();

	ImGui::Begin("scene");
	if (scene) {
		const int selectedId = inspector.getSelectedEntityId();

		for (size_t i = 1; i <= scene->getEntityCount(); ++i) {
			Entity& entity = scene->getEntity(i);

			std::string label = entity.getName().empty() ? ("Entity " + std::to_string(i)) : entity.getName();

			const int thisId = static_cast<int>(entity.getID());
			bool isSelected = (selectedId == thisId);
			if (ImGui::Selectable(label.c_str(), isSelected)) {
				inspector.selectEntity(thisId);
			}

			// Right-click context menu for each entity
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Remove")) {
					scene->removeEntity(i);
					ImGui::EndPopup();
					break; // Entities list changed, break out of loop
				}
				ImGui::EndPopup();
			}
		}

		// SceneUi no longer owns selection or per-entity editing
	} else {
		ImGui::Text("No active scene.");
	}
	ImGui::End();
}
