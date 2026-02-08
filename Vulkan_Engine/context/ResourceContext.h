#pragma once

#include <memory>

class LightManager;
class MaterialManager;

/// Groups resource manager initialization and cleanup
class ResourceContext {
public:
	static void init();
	static void loadDefaults();

	/// Cleanup all resource managers in reverse order of initialization
	static void cleanup();

	// Access to manager instances
	static LightManager& getLightManager();
	static MaterialManager& getMaterialManager();

private:
	static std::unique_ptr<LightManager> lightManager;
	static std::unique_ptr<MaterialManager> materialManager;
};
