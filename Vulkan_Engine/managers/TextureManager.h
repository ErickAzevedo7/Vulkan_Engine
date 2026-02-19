#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "renderer/GraphicsTexture.h"
#include "renderer/RenderTypes.h"

using namespace Renderer;

struct Texture {
	TextureHandle handle;
	SamplerHandle sampler;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
};

struct ThumbnailTexture {
	TextureHandle handle;
	SamplerHandle sampler;
	uint32_t width = 0;
	uint32_t height = 0;
};

class TextureManager {
public:
	TextureManager();
	~TextureManager();

	void setGraphicsTexture(GraphicsTexture* graphicsTexture);
	GraphicsTexture* getGraphicsTexture() const {
		return graphicsTexture;
	}

	void loadDefaults();
	const std::string kDefaultTextureKey = "common/texture/default.png";

	// Load all textures found under assets directory at startup
	void loadAllFromAssets(const std::string& assetsRoot);

	Texture* loadTexture(const std::string& path, bool flipV = true);

	// Samplers are now created via GraphicsTexture, so these might be helpers or wrapped
	void createTextureSampler(Texture* texture);
	void createThumbnailSampler(ThumbnailTexture* texture);
	// createTextureImageView is INTERNAL to GraphicsTexture now

	Texture* getTexture(const std::string& path);
	const ThumbnailTexture* getThumbnail(const std::string& key);

	const std::unordered_map<std::string, Texture>& getAllTextures();

	// Register an existing texture under its file path key in the manager map
	void registerTexture(const std::string& path, const Texture& texture);

	// generateMipmaps is internal to GraphicsTexture/VulkanTexture usually, but kept if needed

private:
	GraphicsTexture* graphicsTexture = nullptr;
	std::unordered_map<std::string, Texture> textures;
	std::unordered_map<std::string, ThumbnailTexture> thumbnails;

	// Create (or reuse existing) downscaled thumbnail texture from a full Texture
	ThumbnailTexture& createThumbnail(const std::string& key, const Texture& sourceTexture);
};
