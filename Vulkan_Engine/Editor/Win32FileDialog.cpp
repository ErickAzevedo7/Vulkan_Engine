// Win32FileDialog.cpp
// Isolated windows headers to avoid conflicts.
#include "Win32FileDialog.h"

// clang-format off
#include <minwindef.h>
#include <windows.h>
#include <commdlg.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <string>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

std::string win32_ShowSaveDialog(GLFWwindow* window, const char* initialDir) {
	char filePath[MAX_PATH] = "scene.iscene";

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = glfwGetWin32Window(window);
	ofn.lpstrFilter = "Scene Files (*.iscene)\0*.iscene\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initialDir;
	ofn.lpstrTitle = "Save Scene";
	ofn.lpstrDefExt = "iscene";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameA(&ofn))
		return std::string(filePath);
	return "";
}

std::string win32_ShowOpenDialog(GLFWwindow* window, const char* initialDir) {
	char filePath[MAX_PATH] = {};

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = glfwGetWin32Window(window);
	ofn.lpstrFilter = "Scene Files (*.iscene)\0*.iscene\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initialDir;
	ofn.lpstrTitle = "Open Scene";
	ofn.lpstrDefExt = "iscene";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
		return std::string(filePath);
	return "";
}
