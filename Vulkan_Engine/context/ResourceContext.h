#pragma once

/// Context class that manages resource-related managers
/// Groups TextureManager, MaterialManager, and LightManager
/// to simplify initialization and cleanup
class ResourceContext {
public:
	/// Initialize all resource managers in the correct order
	/// This sets up the infrastructure for textures, materials, and lighting
	static void init();

	/// Load default resources (textures, materials)
	/// Should be called after init()
	static void loadDefaults();

	/// Cleanup all resource managers in reverse order of initialization
	static void cleanup();
};
