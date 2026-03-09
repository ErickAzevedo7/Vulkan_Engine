#pragma once

#include <GLFW/glfw3.h>

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

	ResourceContext& resourceContext;
	GLFWwindow* window;
};
