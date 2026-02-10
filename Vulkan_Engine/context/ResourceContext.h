#pragma once

#include <memory>

class LightManager;
class MaterialManager;
class SceneManager;
class TextureManager;

/// Groups resource manager initialization and cleanup
class ResourceContext {
public:
	ResourceContext();
	~ResourceContext();

	void init(); // Initialize managers (create Vulkan resources)
	void cleanup(); // Explicitly cleanup managers (needed before device destruction)
	void loadDefaults();

	// Access to manager instances
	LightManager& getLightManager();
	MaterialManager& getMaterialManager();
	TextureManager& getTextureManager();
	SceneManager& getSceneManager();

private:
	std::unique_ptr<LightManager> lightManager;
	std::unique_ptr<MaterialManager> materialManager;
	std::unique_ptr<TextureManager> textureManager;
	std::unique_ptr<SceneManager> sceneManager;
};
