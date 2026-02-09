#pragma once

#include <memory>

class LightManager;
class MaterialManager;
class SceneManager;
class TextureManager;

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
	static TextureManager& getTextureManager();
	static SceneManager& getSceneManager();

private:
	static std::unique_ptr<LightManager> lightManager;
	static std::unique_ptr<MaterialManager> materialManager;
	static std::unique_ptr<TextureManager> textureManager;
	static std::unique_ptr<SceneManager> sceneManager;
};
