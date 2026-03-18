#include "ScriptRegistry.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "components/ScriptComponent.h"
#include "engine_api/EngineExport.h"

// ---------------------------------------------------------------------------
// Private storage — function-local static, initialised once.
// ---------------------------------------------------------------------------
std::unordered_map<std::string, std::function<ScriptComponent*()>>& ScriptRegistry::registry() {
	static std::unordered_map<std::string, std::function<ScriptComponent*()>> instance;
	return instance;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
ScriptComponent* ScriptRegistry::create(const std::string& name) {
	auto it = registry().find(name);
	if (it == registry().end()) {
		std::cerr << "[ScriptRegistry] Unknown script type: '" << name << "'\n";
		return nullptr;
	}
	return it->second();
}

std::vector<std::string> ScriptRegistry::getRegisteredNames() {
	std::vector<std::string> names;
	names.reserve(registry().size());
	for (const auto& [name, _] : registry()) {
		names.push_back(name);
	}
	std::sort(names.begin(), names.end());
	return names;
}

void ScriptRegistry::clear() {
	registry().clear();
}

// ---------------------------------------------------------------------------
// scriptRegistry_register — exported C-linkage free function.
// Called by ScriptAutoRegistrar in DLL plugins; forwards into the registry
// that lives inside the engine EXE, so both DLL and EXE share one map.
// ---------------------------------------------------------------------------
extern "C" ENGINE_API void scriptRegistry_register(const char* name, ScriptComponent* (*factory)()) {
	auto& reg = ScriptRegistry::registry();
	if (reg.count(name)) {
		std::cout << "[ScriptRegistry] Overwriting existing registration: " << name << "\n";
	}
	reg[name] = factory; // Implicitly converts raw pointer to std::function
	std::cout << "[ScriptRegistry] Registered script: " << name << "\n";
}
