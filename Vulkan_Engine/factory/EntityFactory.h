#pragma once

#include <string>

#include "glm/ext/vector_float3.hpp"

// Forward declarations
class Scene;
class Entity;
class ResourceContext;
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
	/// @param resources The resource context for accessing materials/meshes
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @param meshName The mesh to use (e.g., "cube", "sphere", "quad")
	/// @return Reference to the created entity
	static Entity&
	createPrimitive(ResourceContext& resources, Scene* scene, const std::string& name, const std::string& meshName);

	/// Create a light entity with transform, mesh (for visualization), and light component
	/// @param resources The resource context
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @param type The type of light (Directional, Point, Spot)
	/// @param position The position of the light
	/// @param color The color of the light
	/// @param intensity The intensity of the light
	/// @return Reference to the created entity
	static Entity& createLight(ResourceContext& resources,
							   Scene* scene,
							   const std::string& name,
							   LightType type,
							   const glm::vec3& position = glm::vec3(0.0f),
							   const glm::vec3& color = glm::vec3(1.0f),
							   float intensity = 1.0f);

	/// Create a camera entity with transform and camera component
	/// @param scene The scene to create the entity in
	/// @param name The name of the entity
	/// @param position The initial position
	/// @param isPrimary Whether this is the primary camera
	/// @return Reference to the created entity
	static Entity& createCamera(Scene* scene,
								const std::string& name,
								const glm::vec3& position = glm::vec3(0.0f),
								bool isPrimary = true);

	/// Create an entity from a model file on disk.
	/// Supports OBJ/FBX/GLTF/GLB (through MeshManager import support).
	/// @return Pointer to created entity or nullptr on failure.
	static Entity* createModelFromFile(ResourceContext& resources, Scene* scene, const std::string& filePath);
};
