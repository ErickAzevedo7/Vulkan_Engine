#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "engine_api/EngineExport.h"

class ScriptComponent;

enum class InspectorPropertyType { Float, Int, Bool, Vec3, Vec4 };

struct InspectorProperty {
	std::string name;
	InspectorPropertyType type;
	std::variant<
		float*,
		int*,
		bool*,
		glm::vec3*,
		glm::vec4*
	> ptr;
};

class ENGINE_API InspectorPropertyRegistry {
public:
	static InspectorPropertyRegistry& instance();

	template<typename ClassT, typename FieldT>
	void registerMember(
		const std::string& scriptName,
		const std::string& varName,
		InspectorPropertyType type,
		FieldT ClassT::* member) {
		bindings[scriptName].push_back({varName, type, [member](ScriptComponent* base) -> void* {
			auto* typed = static_cast<ClassT*>(base);
			return static_cast<void*>(&(typed->*member));
		}});
	}

	std::vector<InspectorProperty> getValues(const std::string& scriptName, ScriptComponent* instance) const;
	bool hasProperties(const std::string& scriptName) const;
	void clear();

private:
	InspectorPropertyRegistry() = default;

	struct InspectorBinding {
		std::string name;
		InspectorPropertyType type;
		std::function<void*(ScriptComponent*)> getter;
	};

	std::unordered_map<std::string, std::vector<InspectorBinding>> bindings;
};

struct ENGINE_API InspectorPropertyRegistrar {
	template<typename ClassT, typename FieldT>
	InspectorPropertyRegistrar(
		const char* scriptClassName,
		const char* vn,
		InspectorPropertyType vt,
		FieldT ClassT::* member) {
		InspectorPropertyRegistry::instance().registerMember<ClassT, FieldT>(scriptClassName, vn, vt, member);
	}
};
