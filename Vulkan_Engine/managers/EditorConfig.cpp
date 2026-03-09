#include "EditorConfig.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "nlohmann/json_fwd.hpp"

using json = nlohmann::json;
const std::string CONFIG_FILE = "editor_config.json";

void EditorConfig::setLastScenePath(const std::string& path) {
	json j;
	// Load existing config if it exists
	std::ifstream i(CONFIG_FILE);
	if (i.is_open()) {
		try {
			i >> j;
		} catch (const json::parse_error& e) {
			std::cerr << "Save System: Failed to parse editor config: " << e.what() << "\n";
		}
		i.close();
	}

	// Update the path
	j["last_scene_path"] = path;

	// Save back to file
	std::ofstream o(CONFIG_FILE);
	if (o.is_open()) {
		o << j.dump(4);
	} else {
		std::cerr << "Save System: Failed to save editor config to " << CONFIG_FILE << "\n";
	}
}

std::string EditorConfig::getLastScenePath() {
	std::ifstream i(CONFIG_FILE);
	if (i.is_open()) {
		json j;
		try {
			i >> j;
			if (j.contains("last_scene_path") && j["last_scene_path"].is_string()) {
				return j["last_scene_path"].get<std::string>();
			}
		} catch (const json::parse_error& e) {
			std::cerr << "Save System: Failed to parse editor config: " << e.what() << "\n";
		}
	}
	return "";
}
