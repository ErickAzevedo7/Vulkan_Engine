#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "Component.h"
#include "engine_api/EngineExport.h"
#include "ExposedVariable.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class ENGINE_API ScriptComponent : public Component {
public:
	std::string scriptName;
	std::string headerPath;
	bool enabled = true;

	explicit ScriptComponent(const std::string& name = "MyScript") : scriptName(name) {
	}
	~ScriptComponent() override = default;

	bool hasInspectorProperties() const {
		return InspectorPropertyRegistry::instance().hasProperties(scriptName);
	}

	std::vector<InspectorProperty> getInspectorProperties() const {
		return InspectorPropertyRegistry::instance().getValues(scriptName, const_cast<ScriptComponent*>(this));
	}

	json getInspectorPropertiesJson() const {
		json j = json::object();
		auto vars = getInspectorProperties();
		for (const auto& var : vars) {
			switch (var.type) {
			case InspectorPropertyType::Float:
				j[var.name] = *std::get<float*>(var.ptr);
				break;
			case InspectorPropertyType::Int:
				j[var.name] = *std::get<int*>(var.ptr);
				break;
			case InspectorPropertyType::Bool:
				j[var.name] = *std::get<bool*>(var.ptr);
				break;
			case InspectorPropertyType::Vec3:
				j[var.name] = {std::get<glm::vec3*>(var.ptr)->x, std::get<glm::vec3*>(var.ptr)->y, std::get<glm::vec3*>(var.ptr)->z};
				break;
			case InspectorPropertyType::Vec4:
				j[var.name] = {std::get<glm::vec4*>(var.ptr)->x, std::get<glm::vec4*>(var.ptr)->y, std::get<glm::vec4*>(var.ptr)->z, std::get<glm::vec4*>(var.ptr)->w};
				break;
			}
		}
		return j;
	}

	void setInspectorPropertiesFromJson(const json& j) {
		auto vars = getInspectorProperties();
		for (const auto& var : vars) {
			if (!j.contains(var.name)) continue;
			switch (var.type) {
			case InspectorPropertyType::Float:
				*std::get<float*>(var.ptr) = j[var.name].get<float>();
				break;
			case InspectorPropertyType::Int:
				*std::get<int*>(var.ptr) = j[var.name].get<int>();
				break;
			case InspectorPropertyType::Bool:
				*std::get<bool*>(var.ptr) = j[var.name].get<bool>();
				break;
			case InspectorPropertyType::Vec3:
				std::get<glm::vec3*>(var.ptr)->x = j[var.name][0].get<float>();
				std::get<glm::vec3*>(var.ptr)->y = j[var.name][1].get<float>();
				std::get<glm::vec3*>(var.ptr)->z = j[var.name][2].get<float>();
				break;
			case InspectorPropertyType::Vec4:
				std::get<glm::vec4*>(var.ptr)->x = j[var.name][0].get<float>();
				std::get<glm::vec4*>(var.ptr)->y = j[var.name][1].get<float>();
				std::get<glm::vec4*>(var.ptr)->z = j[var.name][2].get<float>();
				std::get<glm::vec4*>(var.ptr)->w = j[var.name][3].get<float>();
				break;
			}
		}
	}

	void onStart() override {
		if (!enabled)
			return;
		std::cout << "[Script: " << scriptName << "] onStart\n";
	}
	void onUpdate(float dt) override {
		(void)dt;
	}
	void onStop() override {
		if (!enabled)
			return;
		std::cout << "[Script: " << scriptName << "] onStop\n";
	}
};
