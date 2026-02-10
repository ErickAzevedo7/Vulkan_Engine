#include "EntityFactory.h"

#include <stdexcept>
#include <string>

#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/MaterialManager.h"
#include "Scene.h"

#include "glm/ext/vector_float3.hpp"


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
	MeshComponent* meshComp = new MeshComponent(&entity, meshName);
	// Set default material
	if (auto* mat = resources.getMaterialManager().getMaterial("common/material/default.mat")) {
		meshComp->SetMaterial(mat);
	}
	entity.addComponent(meshComp);

	// Add transform component
	Transform* transformComp = new Transform();
	entity.addComponent(transformComp);

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

	// Add transform component
	Transform* transform = new Transform();
	transform->position = position;
	transform->scale = glm::vec3(0.5f); // Smaller scale for light visualization
	entity.addComponent(transform);

	// Add mesh component for visualization (small cube)
	MeshComponent* meshComp = new MeshComponent(&entity, "cube");
	// Set default material
	if (auto* mat = resources.getMaterialManager().getMaterial("common/material/default.mat")) {
		meshComp->SetMaterial(mat);
	}
	entity.addComponent(meshComp);

	// Add light component
	LightComponent* lightComp = new LightComponent(&entity, type);
	lightComp->color = color;
	lightComp->intensity = intensity;
	entity.addComponent(lightComp);

	return entity;
}
