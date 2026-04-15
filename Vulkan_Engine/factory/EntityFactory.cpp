#include "EntityFactory.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "components/CameraComponent.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/StaticMeshColliderComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "core/vulkancore.h"
#include "Scene.h"

#include "glm/ext/vector_float3.hpp"

namespace {
struct NaturalComparator {
	bool operator()(const std::string& a, const std::string& b) const {
		auto it1 = a.begin();
		auto it2 = b.begin();
		while (it1 != a.end() && it2 != b.end()) {
			if (std::isdigit(static_cast<unsigned char>(*it1)) && std::isdigit(static_cast<unsigned char>(*it2))) {
				char* end1 = nullptr;
				char* end2 = nullptr;
				long n1 = std::strtol(&*it1, &end1, 10);
				long n2 = std::strtol(&*it2, &end2, 10);
				if (n1 != n2) {
					return n1 < n2;
				}
				it1 += (end1 - &*it1);
				it2 += (end2 - &*it2);
			} else {
				if (*it1 != *it2) {
					return *it1 < *it2;
				}
				++it1;
				++it2;
			}
		}
		return a.length() < b.length();
	}
};
} // namespace


Entity& EntityFactory::createEmpty(Scene* scene, const std::string& name) {
	if (!scene) {
		throw std::runtime_error("Cannot create entity: scene is null");
	}
	return scene->createEntity(name);
}

Entity& EntityFactory::createPrimitive(ResourceContext& resources,
									   Scene* scene,
									   const std::string& name,
									   const std::string& meshName) {
	if (!scene) {
		throw std::runtime_error("Cannot create entity: scene is null");
	}

	Entity& entity = scene->createEntity(name);

	// Add mesh component
	MeshComponent* meshComp = new MeshComponent(&entity, meshName, resources.getMeshManager());
	// Set default material
	if (auto* mat = resources.getMaterialManager().getMaterial("common/material/default.mat")) {
		meshComp->SetMaterial(mat);
	}
	entity.addComponent(meshComp);

	return entity;
}

Entity& EntityFactory::createLight(ResourceContext& resources,
								   Scene* scene,
								   const std::string& name,
								   LightType type,
								   const glm::vec3& position,
								   const glm::vec3& color,
								   float intensity) {
	if (!scene) {
		throw std::runtime_error("Cannot create entity: scene is null");
	}

	Entity& entity = scene->createEntity(name);

	if (Transform* transform = entity.getComponent<Transform>()) {
		transform->position = position;
		transform->scale = glm::vec3(0.5f); // Smaller scale for light visualization
	}

	// Add mesh component for visualization (small cube)
	// Commented out to prevent the light source from casting a shadow when pointing straight down!
	// MeshComponent* meshComp = new MeshComponent(&entity, "cube", resources.getMeshManager());
	// Set default material
	// if (auto* mat = resources.getMaterialManager().getMaterial("common/material/default.mat")) {
	// 	meshComp->SetMaterial(mat);
	// }
	// entity.addComponent(meshComp);

	// Add light component
	LightComponent* lightComp = new LightComponent(&entity, type);
	lightComp->color = color;
	lightComp->intensity = intensity;
	entity.addComponent(lightComp);

	return entity;
}

Entity& EntityFactory::createCamera(Scene* scene, const std::string& name, const glm::vec3& position, bool isPrimary) {
	if (!scene) {
		throw std::runtime_error("Cannot create entity: scene is null");
	}

	Entity& entity = scene->createEntity(name);

	if (Transform* transform = entity.getComponent<Transform>()) {
		transform->position = position;
	}

	// Add camera component
	CameraComponent* camera = new CameraComponent(&entity);
	camera->isPrimary = isPrimary;
	entity.addComponent(camera);

	return entity;
}

Entity* EntityFactory::createModelFromFile(ResourceContext& resources, Scene* scene, const std::string& filePath) {
	if (!scene) {
		return nullptr;
	}

	MeshManager& meshManager = resources.getMeshManager();
	std::vector<std::string> partMeshNames = meshManager.loadMeshPartsFromFile(filePath, VulkanCore::getCommandPool());
	if (partMeshNames.empty()) {
		return nullptr;
	}

	std::string entityName = std::filesystem::path(filePath).stem().string();
	if (entityName.empty()) {
		entityName = "Model";
	}

	Entity& entity = scene->createEntity(entityName);

	auto* defaultMaterial = resources.getMaterialManager().getMaterial("common/material/default.mat");
	auto addMeshAndCollider = [&](Entity& target, const std::string& meshName) {
		auto* meshComp = new MeshComponent(&target, meshName, meshManager);
		if (defaultMaterial) {
			meshComp->SetMaterial(defaultMaterial);
		}
		target.addComponent(meshComp);

		auto* staticMeshCollider = new StaticMeshColliderComponent();
		staticMeshCollider->isTrigger = false;
		staticMeshCollider->useAttachedMeshBounds = true;
		target.addComponent(staticMeshCollider);
	};

	if (partMeshNames.size() == 1) {
		addMeshAndCollider(entity, partMeshNames[0]);
		return &entity;
	}

	struct PartEntry {
		std::string meshName;
		std::string childName;
		size_t importOrder;
	};

	std::vector<PartEntry> entries;
	entries.reserve(partMeshNames.size());

	for (size_t i = 0; i < partMeshNames.size(); ++i) {
		const std::string& partMeshName = partMeshNames[i];
		std::string childName = "Part" + std::to_string(i);
		size_t separatorPos = partMeshName.rfind("::");
		if (separatorPos != std::string::npos && separatorPos + 2 < partMeshName.size()) {
			childName = partMeshName.substr(separatorPos + 2);
		}
		entries.push_back({partMeshName, childName, i});
	}

	NaturalComparator naturalComparator;
	std::stable_sort(entries.begin(), entries.end(), [&](const PartEntry& a, const PartEntry& b) {
		return naturalComparator(a.childName, b.childName);
	});

	for (const PartEntry& entry : entries) {
		const std::string& partMeshName = entry.meshName;
		const std::string& childName = entry.childName;

		Entity& child = scene->createEntity(childName);
		child.setParent(&entity, true);
		addMeshAndCollider(child, partMeshName);
	}

	return &entity;
}
