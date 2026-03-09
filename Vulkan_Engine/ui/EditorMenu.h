#pragma once

#include <GLFW/glfw3.h>
#include <string>

#include "context/ResourceContext.h"


class EditorMenu {
public:
	EditorMenu(ResourceContext& resourceContext, GLFWwindow* window = nullptr);
	void setWindow(GLFWwindow* w);
	void render();
	void loadLastScene();

private:
	void newScene();
	void quickSave();
	void saveAs();
	void openLoadDialog();

	void onPlay();
	void onStop();

	ResourceContext& resourceContext;
	GLFWwindow* window;

	// Temporarily store the scene path to restore when stopping play
	std::string editorSceneBackupPath;
};
