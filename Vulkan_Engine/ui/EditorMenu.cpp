#include "EditorMenu.h"

#include <filesystem>
#include <stdlib.h>
#include <string>

#include "components/LightComponent.h"
#include "Editor/Win32FileDialog.h"
#include "factory/EntityFactory.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "managers/EditorConfig.h"
#include "managers/ProjectSerializer.h"
#include "managers/SceneManager.h"

#include "glm/ext/vector_float3.hpp"

EditorMenu::EditorMenu(ResourceContext& resourceContext, GLFWwindow* window)
	: resourceContext(resourceContext), window(window) {
}

void EditorMenu::setWindow(GLFWwindow* w) {
	window = w;
}

void EditorMenu::render() {
	if (ImGui::BeginMainMenuBar()) {
		// --- File Menu ---
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
				newScene();
			}
			if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
				openLoadDialog();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				quickSave();
			}
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
				saveAs();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Entities")) {
			if (ImGui::MenuItem("Create Empty Entity")) {
				EntityFactory::createEmpty(resourceContext.getSceneManager().getActiveScene(), "Empty Entity");
			}

			if (ImGui::MenuItem("Create Cube")) {
				EntityFactory::createPrimitive(
					resourceContext, resourceContext.getSceneManager().getActiveScene(), "Cube", "cube");
			}
			if (ImGui::MenuItem("Create Sphere")) {
				EntityFactory::createPrimitive(
					resourceContext, resourceContext.getSceneManager().getActiveScene(), "Sphere", "sphere");
			}
			if (ImGui::MenuItem("Create Quad")) {
				EntityFactory::createPrimitive(
					resourceContext, resourceContext.getSceneManager().getActiveScene(), "Quad", "quad");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// --- Keyboard shortcuts ---
	{
		ImGuiIO& io = ImGui::GetIO();
		bool ctrl = io.KeyCtrl;
		bool shift = io.KeyShift;

		if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
			quickSave();
		if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
			saveAs();
		if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O, false))
			openLoadDialog();
		if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false))
			newScene();
	}
}

void EditorMenu::newScene() {
	Scene* scene = resourceContext.getSceneManager().getActiveScene();
	if (scene) {
		// Ask the user to specify a filename for their new scene
		std::filesystem::create_directories("projects");
		char absBase[512];
		_fullpath(absBase, "projects", sizeof(absBase));

		std::string baseFilename = "new_scene";
		std::string ext = ".iscene";
		std::string path = std::string(absBase) + "\\" + baseFilename + ext;

		int counter = 1;
		while (std::filesystem::exists(path)) {
			path = std::string(absBase) + "\\" + baseFilename + "(" + std::to_string(counter) + ")" + ext;
			counter++;
		}

		// Clear the current active scene
		scene->clear();

		// Fallback generic light so the scene isn't completely dark
		Entity& lightEntity = EntityFactory::createLight(resourceContext,
														 scene,
														 "Directional Light",
														 LightType::Directional,
														 glm::vec3(0.0f, 10.0f, 0.0f),
														 glm::vec3(1.0f, 0.95f, 0.9f),
														 10.0f);

		// Set the direction for the directional light
		LightComponent* lc = lightEntity.getComponent<LightComponent>();
		if (lc) {
			lc->direction = glm::vec3(0.0f, -1.0f, 0.0f);
			lc->range = 20.0f; // shadow far plane
		}

		// Immediately save the newly created empty scene
		ProjectSerializer::save(path, scene, resourceContext);
		EditorConfig::setLastScenePath(path);
	}
}

void EditorMenu::quickSave() {
	std::string lastScene = EditorConfig::getLastScenePath();
	if (!lastScene.empty()) {
		ProjectSerializer::save(lastScene, resourceContext.getSceneManager().getActiveScene(), resourceContext);
	} else {
		saveAs();
	}
}

void EditorMenu::saveAs() {
	// Ensure projects/ directory exists so the dialog starts there
	std::filesystem::create_directories("projects");
	char absBase[512];
	_fullpath(absBase, "projects", sizeof(absBase));

	std::string path = win32_ShowSaveDialog(window, absBase);
	if (!path.empty()) {
		ProjectSerializer::save(path, resourceContext.getSceneManager().getActiveScene(), resourceContext);
		EditorConfig::setLastScenePath(path);
	}
}

void EditorMenu::openLoadDialog() {
	char absBase[512];
	_fullpath(absBase, "projects", sizeof(absBase));

	std::string path = win32_ShowOpenDialog(window, absBase);
	if (!path.empty()) {
		ProjectSerializer::load(path, resourceContext.getSceneManager().getActiveScene(), resourceContext);
		EditorConfig::setLastScenePath(path);
	}
}

void EditorMenu::loadLastScene() {
	// Try loading the last opened scene
	std::string lastScene = EditorConfig::getLastScenePath();
	Scene* scene = resourceContext.getSceneManager().getActiveScene();
	if (scene) {
		if (!lastScene.empty() && std::filesystem::exists(lastScene)) {
			ProjectSerializer::load(lastScene, scene, resourceContext);
		} else {
			// Fallback to creating a brand new scene file automatically
			newScene();
		}
	}
}
