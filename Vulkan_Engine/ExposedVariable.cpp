#include "ExposedVariable.h"
#include "components/ScriptComponent.h"

InspectorPropertyRegistry& InspectorPropertyRegistry::instance() {
	static InspectorPropertyRegistry inst;
	return inst;
}

std::vector<InspectorProperty> InspectorPropertyRegistry::getValues(const std::string& scriptName, ScriptComponent* instance) const {
	auto it = bindings.find(scriptName);
	if (it != bindings.end()) {
		std::vector<InspectorProperty> values;
		values.reserve(it->second.size());
		for (const auto& binding : it->second) {
			void* ptr = binding.getter(instance);
			switch (binding.type) {
			case InspectorPropertyType::Float:
				values.push_back({binding.name, binding.type, reinterpret_cast<float*>(ptr)});
				break;
			case InspectorPropertyType::Int:
				values.push_back({binding.name, binding.type, reinterpret_cast<int*>(ptr)});
				break;
			case InspectorPropertyType::Bool:
				values.push_back({binding.name, binding.type, reinterpret_cast<bool*>(ptr)});
				break;
			case InspectorPropertyType::Vec3:
				values.push_back({binding.name, binding.type, reinterpret_cast<glm::vec3*>(ptr)});
				break;
			case InspectorPropertyType::Vec4:
				values.push_back({binding.name, binding.type, reinterpret_cast<glm::vec4*>(ptr)});
				break;
			}
		}
		return values;
	}
	return {};
}

bool InspectorPropertyRegistry::hasProperties(const std::string& scriptName) const {
	return bindings.find(scriptName) != bindings.end();
}

void InspectorPropertyRegistry::clear() {
	bindings.clear();
}
