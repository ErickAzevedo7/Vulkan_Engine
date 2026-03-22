#include "ScriptPluginLoader.h"

#include <iostream>
#include <string>

#include "ExposedVariable.h"
#include "ScriptRegistry.h"

#ifdef _WIN32
std::vector<HMODULE> ScriptPluginLoader::loadedModules;
#endif

bool ScriptPluginLoader::loadPlugin(const std::string& dllPath) {
#ifdef _WIN32
	HMODULE handle = LoadLibraryA(dllPath.c_str());
	if (!handle) {
		DWORD err = GetLastError();
		std::cerr << "[ScriptPluginLoader] Failed to load DLL '" << dllPath << "' (error " << err << ")\n";
		return false;
	}

	// The SCRIPT() macro auto-registers scripts via static initializers in the DLL.
	// Calling registerScripts() triggers any explicit registration code as well.
	RegisterFn registerFn = reinterpret_cast<RegisterFn>(GetProcAddress(handle, "registerScripts"));

	if (!registerFn) {
		// Not a script DLL — not an error, static initializers already ran
		FreeLibrary(handle);
		return false;
	}

	registerFn();
	loadedModules.push_back(handle);
	std::cout << "[ScriptPluginLoader] Loaded: " << dllPath << "\n";
	return true;
#else
	(void)dllPath;
	std::cerr << "[ScriptPluginLoader] DLL loading not supported on this platform.\n";
	return false;
#endif
}

void ScriptPluginLoader::unloadAll() {
#ifdef _WIN32
	// CRITICAL: Clear registry map first!
	// This destroys all std::function objects pointing into the DLLs while the code is still mapped.
	ScriptRegistry::clear();
	InspectorPropertyRegistry::instance().clear();

	for (HMODULE h : loadedModules) {
		FreeLibrary(h);
	}
	loadedModules.clear();
	std::cout << "[ScriptPluginLoader] All script DLLs unloaded safely.\n";
#endif
}
