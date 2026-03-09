#pragma once
#include <string>

class EditorConfig {
public:
	static void setLastScenePath(const std::string& path);
	static std::string getLastScenePath();
};
