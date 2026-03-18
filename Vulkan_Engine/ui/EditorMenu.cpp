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
#include "Scene.h"

#include "glm/ext/vector_float3.hpp"


EditorMenu::EditorMenu(ResourceContext& resourceContext, GLFWwindow* window)
	: resourceContext(resourceContext), window(window) {
}

void EditorMenu::setWindow(GLFWwindow* w) {
	window = w;
}

void EditorMenu::render() {
	if (window) {
		Scene* scene = resourceContext.getSceneManager().getActiveScene();
		std::string lastScene = EditorConfig::getLastScenePath();
		std::string title = "Vulkan Engine";
		if (!lastScene.empty()) {
			std::filesystem::path p(lastScene);
			title += " - " + p.filename().string();
		}
		if (scene && scene->getIsDirty()) {
			title += " *";
		}
		glfwSetWindowTitle(window, title.c_str());
	}

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
			ImGui::Separator();
			if (ImGui::MenuItem("Create Camera")) {
				EntityFactory::createCamera(resourceContext.getSceneManager().getActiveScene(), "Camera");
			}
			ImGui::EndMenu();
		}

		Scene* scene = resourceContext.getSceneManager().getActiveScene();
		bool isPlaying = scene && scene->getState() == SceneState::Play;

		ImGui::SameLine(ImGui::GetWindowWidth() / 2.0f - 20.0f);
		if (isPlaying) {
			if (ImGui::MenuItem("Stop")) {
				onStop();
			}
		} else {
			if (ImGui::MenuItem("Play")) {
				onPlay();
			}
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

		// Mark as dirty so the user can save to the chosen path when ready
		scene->markDirty();
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

void EditorMenu::onPlay() {
	Scene* scene = resourceContext.getSceneManager().getActiveScene();
	if (!scene)
		return;

	if (scene->getState() == SceneState::Edit) {
		// Save current scene to a temporary backup
		std::filesystem::create_directories("projects/temp");
		editorSceneBackupPath = "projects/temp/play_backup.iscene";

		// Silent save: don't clear the dirty flag so we don't lose the "*" in the title
		ProjectSerializer::save(editorSceneBackupPath, scene, resourceContext, false);

		scene->onRuntimeStart();
	}
}

void EditorMenu::onStop() {
	Scene* scene = resourceContext.getSceneManager().getActiveScene();
	if (!scene)
		return;

	if (scene->getState() == SceneState::Play) {
		scene->onRuntimeStop();

		// Reload the backup
		if (!editorSceneBackupPath.empty() && std::filesystem::exists(editorSceneBackupPath)) {
			ProjectSerializer::load(editorSceneBackupPath, scene, resourceContext);
		}
	}
}
