#include "PrefabSerializer.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "components/CameraComponent.h"
#include "components/ColliderComponent.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/ScriptComponent.h"
#include "components/StaticMeshColliderComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "core/vulkancore.h"
#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "managers/ScriptCompiler.h"
#include "managers/ScriptRegistry.h"
#include "Scene.h"

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace {

void writeEntityRecursive(const Entity& entity, json& outEntityJson) {
	outEntityJson["name"] = entity.getName();

	if (const auto* t = entity.getComponent<Transform>()) {
		json tj;
		tj["position"] = {t->position.x, t->position.y, t->position.z};
		tj["rotation"] = {t->rotation.w, t->rotation.x, t->rotation.y, t->rotation.z};
		tj["scale"] = {t->scale.x, t->scale.y, t->scale.z};
		outEntityJson["transform"] = tj;
	}

	if (const auto* m = entity.getComponent<MeshComponent>()) {
		json mj;
		Mesh* mesh = m->GetMesh();
		mj["mesh_name"] = mesh ? mesh->name : "";
		Material* mat = m->GetMaterial();
		mj["material_path"] = mat ? mat->filePath : "";
		outEntityJson["mesh"] = mj;
	}

	if (const auto* lc = entity.getComponent<LightComponent>()) {
		json lj;
		lj["type"] = static_cast<int>(lc->getType());
		lj["color"] = {lc->color.r, lc->color.g, lc->color.b};
		lj["intensity"] = lc->intensity;
		lj["range"] = lc->range;
		lj["direction"] = {lc->direction.x, lc->direction.y, lc->direction.z};
		lj["innerConeAngle"] = lc->innerConeAngle;
		lj["outerConeAngle"] = lc->outerConeAngle;
		lj["Kc"] = lc->attenuationKc;
		lj["Kl"] = lc->attenuationKl;
		lj["Kq"] = lc->attenuationKq;
		outEntityJson["light"] = lj;
	}

	if (const auto* cc = entity.getComponent<CameraComponent>()) {
		json cj;
		cj["fov"] = cc->fov;
		cj["nearPlane"] = cc->nearPlane;
		cj["farPlane"] = cc->farPlane;
		cj["isPrimary"] = cc->isPrimary;
		outEntityJson["camera"] = cj;
	}

	auto scriptComponents = entity.getComponents<ScriptComponent>();
	if (!scriptComponents.empty()) {
		json scripts = json::array();
		for (const auto* sc : scriptComponents) {
			json sj;
			sj["name"] = sc->scriptName;
			sj["header_path"] = sc->headerPath;
			sj["enabled"] = sc->enabled;
			sj["inspector_props"] = sc->getInspectorPropertiesJson();
			scripts.push_back(sj);
		}
		outEntityJson["scripts"] = scripts;
	}

	if (const auto* collider = entity.getComponent<ColliderComponent>()) {
		json cj;
		cj["enabled"] = collider->enabled;
		cj["isTrigger"] = collider->isTrigger;
		cj["isStatic"] = collider->isStatic;
		cj["center"] = {collider->center.x, collider->center.y, collider->center.z};
		cj["size"] = {collider->size.x, collider->size.y, collider->size.z};
		outEntityJson["collider"] = cj;
	}

	if (const auto* staticMeshCollider = entity.getComponent<StaticMeshColliderComponent>()) {
		json smj;
		smj["enabled"] = staticMeshCollider->enabled;
		smj["isTrigger"] = staticMeshCollider->isTrigger;
		smj["useAttachedMeshBounds"] = staticMeshCollider->useAttachedMeshBounds;
		smj["localCenter"] = {
			staticMeshCollider->localCenter.x,
			staticMeshCollider->localCenter.y,
			staticMeshCollider->localCenter.z
		};
		smj["localSize"] = {
			staticMeshCollider->localSize.x,
			staticMeshCollider->localSize.y,
			staticMeshCollider->localSize.z
		};
		outEntityJson["static_mesh_collider"] = smj;
	}

	outEntityJson["children"] = json::array();
	for (Entity* child : entity.getChildren()) {
		if (!child) {
			continue;
		}
		json childJson;
		writeEntityRecursive(*child, childJson);
		outEntityJson["children"].push_back(childJson);
	}
}

Entity* readEntityRecursive(const json& entityJson,
							Scene* scene,
							ResourceContext& resources,
							const std::string* rootNameOverride,
							Entity* parent) {
	if (!scene) {
		return nullptr;
	}

	const std::string defaultName = entityJson.value("name", "Entity");
	const std::string entityName = (rootNameOverride && !rootNameOverride->empty()) ? *rootNameOverride : defaultName;
	Entity& entity = scene->createEntity(entityName);

	if (parent) {
		entity.setParent(parent, false);
	}

	if (entityJson.contains("transform")) {
		const auto& tj = entityJson["transform"];
		Transform* t = entity.getComponent<Transform>();
		if (t && tj.contains("position") && tj["position"].size() == 3) {
			t->position = {tj["position"][0], tj["position"][1], tj["position"][2]};
		}
		if (t && tj.contains("rotation") && tj["rotation"].size() == 4) {
			t->rotation = glm::quat(tj["rotation"][0], tj["rotation"][1], tj["rotation"][2], tj["rotation"][3]);
		}
		if (t && tj.contains("scale") && tj["scale"].size() == 3) {
			t->scale = {tj["scale"][0], tj["scale"][1], tj["scale"][2]};
		}
	}

	MeshManager& meshMgr = resources.getMeshManager();
	MaterialManager& matMgr = resources.getMaterialManager();

	if (entityJson.contains("mesh")) {
		const auto& mj = entityJson["mesh"];
		const std::string meshName = mj.value("mesh_name", "");
		const std::string matPath = mj.value("material_path", "");
		if (!meshName.empty()) {
			Mesh* mesh = meshMgr.getMesh(meshName);
			if (!mesh && MeshManager::isSupportedModelFile(meshName)) {
				mesh = meshMgr.loadMeshFromFile(meshName, VulkanCore::getCommandPool());
			}
			if (mesh) {
				(void)mesh;
				auto* mc = new MeshComponent(&entity, meshName, meshMgr);
				Material* mat = matPath.empty() ? nullptr : matMgr.getMaterial(matPath);
				if (!mat) {
					mat = matMgr.getMaterial("common/material/default.mat");
				}
				if (mat) {
					mc->SetMaterial(mat);
				}
				entity.addComponent(mc);
			} else {
				std::cerr << "[PrefabSerializer] Mesh not found: " << meshName << "\n";
			}
		}
	}

	if (entityJson.contains("light")) {
		const auto& lj = entityJson["light"];
		LightType type = static_cast<LightType>(lj.value("type", 1));
		auto* lc = new LightComponent(&entity, type);
		if (lj.contains("color") && lj["color"].size() == 3) {
			lc->color = {lj["color"][0], lj["color"][1], lj["color"][2]};
		}
		lc->intensity = lj.value("intensity", 1.0f);
		lc->range = lj.value("range", 50.0f);
		if (lj.contains("direction") && lj["direction"].size() == 3) {
			lc->direction = {lj["direction"][0], lj["direction"][1], lj["direction"][2]};
		}
		lc->innerConeAngle = lj.value("innerConeAngle", lc->innerConeAngle);
		lc->outerConeAngle = lj.value("outerConeAngle", lc->outerConeAngle);
		lc->attenuationKc = lj.value("Kc", 1.0f);
		lc->attenuationKl = lj.value("Kl", 0.09f);
		lc->attenuationKq = lj.value("Kq", 0.032f);
		entity.addComponent(lc);
	}

	if (entityJson.contains("camera")) {
		const auto& cj = entityJson["camera"];
		auto* cc = new CameraComponent(&entity);
		cc->fov = cj.value("fov", 45.0f);
		cc->nearPlane = cj.value("nearPlane", 0.1f);
		cc->farPlane = cj.value("farPlane", 1000.0f);
		cc->isPrimary = cj.value("isPrimary", true);
		entity.addComponent(cc);
	}

	auto loadScriptEntry = [&](const json& sj) {
		std::string scriptName = sj.value("name", "MyScript");
		std::string headerPath = sj.value("header_path", "");
		bool enabled = sj.value("enabled", true);

		if (!headerPath.empty()) {
			if (!ScriptCompiler::loadFromHeader(headerPath)) {
				std::cerr << "[PrefabSerializer] Failed to load script from header: " << headerPath << "\n";
			}
		}

		ScriptComponent* sc = ScriptRegistry::create(scriptName);
		if (sc) {
			sc->headerPath = headerPath;
			sc->enabled = enabled;
			if (sj.contains("inspector_props")) {
				sc->setInspectorPropertiesFromJson(sj["inspector_props"]);
			}
			entity.addComponent(sc);
		}
	};

	if (entityJson.contains("scripts") && entityJson["scripts"].is_array()) {
		for (const auto& sj : entityJson["scripts"]) {
			loadScriptEntry(sj);
		}
	}

	if (entityJson.contains("collider")) {
		const auto& cj = entityJson["collider"];
		auto* collider = new ColliderComponent();
		collider->enabled = cj.value("enabled", true);
		collider->isTrigger = cj.value("isTrigger", true);
		collider->isStatic = cj.value("isStatic", false);
		if (cj.contains("center") && cj["center"].size() == 3) {
			collider->center = {cj["center"][0], cj["center"][1], cj["center"][2]};
		}
		if (cj.contains("size") && cj["size"].size() == 3) {
			collider->size = {cj["size"][0], cj["size"][1], cj["size"][2]};
		}
		entity.addComponent(collider);
	}

	if (entityJson.contains("static_mesh_collider")) {
		const auto& smj = entityJson["static_mesh_collider"];
		auto* staticMeshCollider = new StaticMeshColliderComponent();
		staticMeshCollider->enabled = smj.value("enabled", true);
		staticMeshCollider->isTrigger = smj.value("isTrigger", false);
		staticMeshCollider->useAttachedMeshBounds = smj.value("useAttachedMeshBounds", true);
		if (smj.contains("localCenter") && smj["localCenter"].size() == 3) {
			staticMeshCollider->localCenter = {smj["localCenter"][0], smj["localCenter"][1], smj["localCenter"][2]};
		}
		if (smj.contains("localSize") && smj["localSize"].size() == 3) {
			staticMeshCollider->localSize = {smj["localSize"][0], smj["localSize"][1], smj["localSize"][2]};
		}
		entity.addComponent(staticMeshCollider);
	}

	if (entityJson.contains("children") && entityJson["children"].is_array()) {
		for (const auto& childJson : entityJson["children"]) {
			readEntityRecursive(childJson, scene, resources, nullptr, &entity);
		}
	}

	return &entity;
}

} // namespace

bool PrefabSerializer::save(const std::string& filePath, const Entity& rootEntity) {
	namespace fs = std::filesystem;

	try {
		fs::path p(filePath);
		if (p.has_parent_path()) {
			std::error_code ec;
			fs::create_directories(p.parent_path(), ec);
			if (ec) {
				std::cerr << "[PrefabSerializer] Failed to create directory: " << ec.message() << "\n";
				return false;
			}
		}

		json root;
		root["prefab_version"] = 1;
		root["entity"] = json::object();
		writeEntityRecursive(rootEntity, root["entity"]);

		std::ofstream file(filePath, std::ios::trunc);
		if (!file.is_open()) {
			std::cerr << "[PrefabSerializer] Failed to open for writing: " << filePath << "\n";
			return false;
		}
		file << root.dump(4);
		file.close();

		std::cout << "[PrefabSerializer] Saved prefab to: " << filePath << "\n";
		return true;
	} catch (const std::exception& e) {
		std::cerr << "[PrefabSerializer] Save error: " << e.what() << "\n";
		return false;
	}
}

Entity* PrefabSerializer::instantiate(const std::string& filePath,
									  Scene* scene,
									  ResourceContext& resources,
									  const std::string& rootNameOverride) {
	if (!scene) {
		std::cerr << "[PrefabSerializer] instantiate: scene is null\n";
		return nullptr;
	}

	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "[PrefabSerializer] Failed to open prefab: " << filePath << "\n";
		return nullptr;
	}

	json root;
	try {
		file >> root;
	} catch (const std::exception& e) {
		std::cerr << "[PrefabSerializer] JSON parse error: " << e.what() << "\n";
		return nullptr;
	}

	if (!root.contains("entity") || !root["entity"].is_object()) {
		std::cerr << "[PrefabSerializer] Invalid prefab file, missing 'entity' object\n";
		return nullptr;
	}

	Entity* created = readEntityRecursive(root["entity"], scene, resources, &rootNameOverride, nullptr);
	if (created) {
		scene->markDirty();
	}
	return created;
}
