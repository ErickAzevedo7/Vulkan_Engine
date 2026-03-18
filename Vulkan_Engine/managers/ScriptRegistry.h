#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine_api/EngineExport.h"

// Forward declarations
class ScriptComponent;

// ---------------------------------------------------------------------------
// The ONLY cross-DLL registration entry point.
// Declared here because registration belongs to ScriptRegistry.
// Scripts call this via ScriptAutoRegistrar<T> in Behaviour.h.
// ---------------------------------------------------------------------------
extern "C" ENGINE_API void scriptRegistry_register(
	const char* name,
	ScriptComponent* (*factory)());

/// ScriptRegistry
/// Maps script class names to factory functions.
/// Both the engine and external DLLs register here.
class ScriptRegistry {
public:
	/// Create an instance of the named script, or nullptr if not found.
	static ScriptComponent* create(const std::string& name);

	/// All registered script names.
	static std::vector<std::string> getRegisteredNames();

	/// Remove all registrations (called before DLL unload).
	static void clear();

private:
	// Allow the registration hook to access the private map
	friend void ::scriptRegistry_register(const char*, ScriptComponent*(*)());

	static std::unordered_map<std::string, std::function<ScriptComponent*()>>& registry();
};
