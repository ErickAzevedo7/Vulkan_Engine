#pragma once

#include <memory>

class VulkanCore; // Forward declaration for init()

// Forward declarations
class LightManager;
class MaterialManager;
class SceneManager;
class TextureManager;
class MeshManager;

// Renderer abstraction
namespace Renderer {
class GraphicsBuffer; // Use interface
class GraphicsTexture;
class GraphicsResourceBinder;
class GraphicsDevice;
} // namespace Renderer

/// Groups resource manager initialization and cleanup
class ResourceContext {
public:
	ResourceContext();
	~ResourceContext();

	void init(VulkanCore* engineCore); // Initialize managers (create Vulkan resources)
	void cleanup(); // Explicitly cleanup managers (needed before device destruction)
	void loadDefaults();

	// Access to manager instances
	LightManager& getLightManager();
	MaterialManager& getMaterialManager();
	TextureManager& getTextureManager();
	SceneManager& getSceneManager();
	MeshManager& getMeshManager();

	// Access to rendering abstraction
	Renderer::GraphicsDevice& getDevice();
	Renderer::GraphicsBuffer& getBufferManager();
	Renderer::GraphicsResourceBinder& getResourceBinder();

private:
	std::unique_ptr<LightManager> lightManager;
	std::unique_ptr<MaterialManager> materialManager;
	std::unique_ptr<TextureManager> textureManager;
	std::unique_ptr<SceneManager> sceneManager;
	std::unique_ptr<MeshManager> meshManager;

	// Store as interface pointer for true abstraction
	std::unique_ptr<Renderer::GraphicsDevice> graphicsDevice;
	std::unique_ptr<Renderer::GraphicsBuffer> bufferManager;
	std::unique_ptr<Renderer::GraphicsTexture> graphicsTexture; // Texture backend
	std::unique_ptr<Renderer::GraphicsResourceBinder> resourceBinder;
};
