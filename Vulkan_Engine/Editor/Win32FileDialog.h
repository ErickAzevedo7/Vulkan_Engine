// Win32FileDialog.h
// Isolated from Vulkan/GLFW headers to avoid macro conflicts with windows.h
#pragma once
#include <string>

struct GLFWwindow;

/// Opens the native Windows "Save As" dialog, filtered to .iscene files.
/// Returns the chosen path, or empty string if the user cancelled.
std::string win32_ShowSaveDialog(GLFWwindow* window, const char* initialDir);

/// Opens the native Windows "Open" dialog, filtered to .iscene files.
/// Returns the chosen path, or empty string if the user cancelled.
std::string win32_ShowOpenDialog(GLFWwindow* window, const char* initialDir);
