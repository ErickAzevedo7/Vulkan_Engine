#pragma once

#include <string>

#include "glm/ext/vector_float3.hpp"

// Forward declarations
class Scene;
class Entity;
enum class LightType;

/// Factory class for creating common entity types
/// Eliminates code duplication in entity creation
class EntityFactory {
public:
	/// Create an empty entity with just a name
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @return Reference to the created entity
	static Entity& createEmpty(Scene* scene, const std::string& name);

	/// Create a primitive entity (cube, sphere, quad) with mesh and transform
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @param meshName The mesh to use (e.g., "cube", "sphere", "quad")
	/// @return Reference to the created entity
	static Entity& createPrimitive(Scene* scene, const std::string& name, const std::string& meshName);

	/// Create a light entity with transform, mesh (for visualization), and light component
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @param type The type of light (Directional, Point, Spot)
	/// @param position The position of the light
	/// @param color The color of the light
	/// @param intensity The intensity of the light
	/// @return Reference to the created entity
	static Entity& createLight(Scene* scene,
							   const std::string& name,
							   LightType type,
							   const glm::vec3& position = glm::vec3(0.0f),
							   const glm::vec3& color = glm::vec3(1.0f),
							   float intensity = 1.0f);
};
