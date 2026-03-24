#include "EntityFactory.h"

#include <stdexcept>
#include <string>

#include "components/CameraComponent.h"
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
