#pragma once

#include <string>

class Entity;
class Scene;
class ResourceContext;

class PrefabSerializer {
public:
	static bool save(const std::string& filePath, const Entity& rootEntity);
	static Entity* instantiate(const std::string& filePath,
							   Scene* scene,
							   ResourceContext& resources,
							   const std::string& rootNameOverride = "");
};
