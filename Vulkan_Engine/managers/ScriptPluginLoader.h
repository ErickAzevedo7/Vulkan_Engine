#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

/// ScriptPluginLoader
///
/// Loads a compiled per-script DLL explicitly via LoadLibrary
/// and calls its exported registerScripts() function.
///
/// DLLs are produced on-demand by ScriptCompiler when a .h is
/// dragged onto an entity in the Inspector.
class ScriptPluginLoader {
public:
	/// Load a single compiled script DLL.
	/// Calls registerScripts() which triggers SCRIPT() macro self-registration.
	/// Returns true on success.
	static bool loadPlugin(const std::string& dllPath);

	/// Safely unloads all script DLLs. 
	/// Clears ScriptRegistry first to avoid dangling function pointers.
	static void unloadAll();

private:
	using RegisterFn = void(*)();

#ifdef _WIN32
	static std::vector<HMODULE> loadedModules;
#endif
};
