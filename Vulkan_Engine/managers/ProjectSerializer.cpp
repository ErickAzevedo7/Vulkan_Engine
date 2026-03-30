#include "ProjectSerializer.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <string>
#include <system_error>

#include "components/CameraComponent.h"
#include "components/ColliderComponent.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/ScriptComponent.h"
#include "managers/ScriptCompiler.h"
#include "managers/ScriptRegistry.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "Scene.h"

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

using json = nlohmann::json;

bool ProjectSerializer::save(const std::string& filePath, Scene* scene, ResourceContext& resources, bool shouldClearDirty) {
	if (!scene) {
		std::cerr << "[ProjectSerializer] save: scene is null\n";
		return false;
	}

	// Ensure the projects directory exists
	std::filesystem::path p(filePath);
	if (p.has_parent_path()) {
		std::error_code ec;
		std::filesystem::create_directories(p.parent_path(), ec);
		if (ec) {
			std::cerr << "[ProjectSerializer] Failed to create directory: " << ec.message() << "\n";
			return false;
		}
	}

	json root;
	root["scene_name"] = "Scene";
	root["entities"] = json::array();

	auto* entities = scene->getEntities();
	for (const auto& entityPtr : *entities) {
		const Entity& entity = *entityPtr;
		json ej;
		ej["name"] = entity.getName();
		ej["id"] = entity.getID();

		// --- Transform ---
		if (auto* t = entity.getComponent<Transform>()) {
			json tj;
			tj["position"] = {t->position.x, t->position.y, t->position.z};
			tj["rotation"] = {t->rotation.w, t->rotation.x, t->rotation.y, t->rotation.z};
			tj["scale"] = {t->scale.x, t->scale.y, t->scale.z};
			ej["transform"] = tj;
		}

		if (Entity* parent = entity.getParent()) {
			ej["parent_id"] = parent->getID();
		}

		// --- Mesh ---
		if (auto* m = entity.getComponent<MeshComponent>()) {
			json mj;
			Mesh* mesh = m->GetMesh();
			mj["mesh_name"] = mesh ? mesh->name : "";
			Material* mat = m->GetMaterial();
			mj["material_path"] = mat ? mat->filePath : "";
			ej["mesh"] = mj;
		}

		// --- Light ---
		if (auto* lc = entity.getComponent<LightComponent>()) {
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
			ej["light"] = lj;
		}

		// --- Camera ---
		if (auto* cc = entity.getComponent<CameraComponent>()) {
			json cj;
			cj["fov"] = cc->fov;
			cj["nearPlane"] = cc->nearPlane;
			cj["farPlane"] = cc->farPlane;
			cj["isPrimary"] = cc->isPrimary;
			ej["camera"] = cj;
		}

		// --- Script(s) ---
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
			ej["scripts"] = scripts;
		}

		// --- Collider ---
		if (auto* collider = entity.getComponent<ColliderComponent>()) {
			json cj;
			cj["enabled"] = collider->enabled;
			cj["isTrigger"] = collider->isTrigger;
			cj["isStatic"] = collider->isStatic;
			cj["center"] = {collider->center.x, collider->center.y, collider->center.z};
			cj["size"] = {collider->size.x, collider->size.y, collider->size.z};
			ej["collider"] = cj;
		}

		root["entities"].push_back(ej);
	}

	std::ofstream file(filePath, std::ios::trunc);
	if (!file.is_open()) {
		std::cerr << "[ProjectSerializer] Failed to open for writing: " << filePath << "\n";
		return false;
	}
	file << root.dump(4);
	file.close();

	if (shouldClearDirty) {
		scene->clearDirty();
	}

	std::cout << "[ProjectSerializer] Saved scene to: " << filePath << "\n";
	return true;
}

bool ProjectSerializer::load(const std::string& filePath, Scene* scene, ResourceContext& resources) {
	if (!scene) {
		std::cerr << "[ProjectSerializer] load: scene is null\n";
		return false;
	}

	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "[ProjectSerializer] Failed to open: " << filePath << "\n";
		return false;
	}

	json root;
	try {
		file >> root;
	} catch (const std::exception& e) {
		std::cerr << "[ProjectSerializer] JSON parse error: " << e.what() << "\n";
		return false;
	}
	file.close();

	// Clear the existing scene entities
	scene->clear();

	if (!root.contains("entities") || !root["entities"].is_array()) {
		std::cerr << "[ProjectSerializer] No 'entities' array found in file\n";
		return false;
	}

	MeshManager& meshMgr = resources.getMeshManager();
	MaterialManager& matMgr = resources.getMaterialManager();

	std::unordered_map<uint32_t, uint32_t> pendingParentByChildId;

	for (const auto& ej : root["entities"]) {
		std::string name = ej.value("name", "Entity");
		Entity& entity = scene->createEntity(name);

		uint32_t loadedId = entity.getID();
		if (ej.contains("id")) {
			loadedId = ej["id"];
			entity.setID(loadedId);
			Entity::updateNextID(loadedId);
		}

		if (ej.contains("parent_id") && ej["parent_id"].is_number_unsigned()) {
			pendingParentByChildId[loadedId] = ej["parent_id"].get<uint32_t>();
		}

		// --- Transform ---
		if (ej.contains("transform")) {
			const auto& tj = ej["transform"];
			Transform* t = entity.getComponent<Transform>();

			if (t && tj.contains("position") && tj["position"].size() == 3) {
				t->position = {tj["position"][0], tj["position"][1], tj["position"][2]};
			}
			if (t && tj.contains("rotation") && tj["rotation"].size() == 4) {
				// stored as w, x, y, z
				t->rotation = glm::quat(tj["rotation"][0], tj["rotation"][1], tj["rotation"][2], tj["rotation"][3]);
			}
			if (t && tj.contains("scale") && tj["scale"].size() == 3) {
				t->scale = {tj["scale"][0], tj["scale"][1], tj["scale"][2]};
			}
		}

		// --- Mesh ---
		if (ej.contains("mesh")) {
			const auto& mj = ej["mesh"];
			std::string meshName = mj.value("mesh_name", "");
			std::string matPath = mj.value("material_path", "");

			if (!meshName.empty()) {
				Mesh* mesh = meshMgr.getMesh(meshName);
				if (mesh) {
					MeshComponent* mc = new MeshComponent(&entity, meshName, meshMgr);
					// Restore material
					Material* mat = matPath.empty() ? nullptr : matMgr.getMaterial(matPath);
					if (!mat) {
						mat = matMgr.getMaterial("common/material/default.mat");
					}
					if (mat) {
						mc->SetMaterial(mat);
					}
					entity.addComponent(mc);
				} else {
					std::cerr << "[ProjectSerializer] Mesh not found: " << meshName << "\n";
				}
			}
		}

		// --- Light ---
		if (ej.contains("light")) {
			const auto& lj = ej["light"];
			LightType type = static_cast<LightType>(lj.value("type", 1));
			LightComponent* lc = new LightComponent(&entity, type);

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

		// --- Camera ---
		if (ej.contains("camera")) {
			const auto& cj = ej["camera"];
			CameraComponent* cc = new CameraComponent(&entity);
			cc->fov = cj.value("fov", 45.0f);
			cc->nearPlane = cj.value("nearPlane", 0.1f);
			cc->farPlane = cj.value("farPlane", 1000.0f);
			cc->isPrimary = cj.value("isPrimary", true);
			entity.addComponent(cc);
		}

		// --- Script(s) ---
		auto loadScriptEntry = [&](const json& sj) {
			std::string scriptName = sj.value("name", "MyScript");
			std::string headerPath = sj.value("header_path", "");
			bool enabled = sj.value("enabled", true);

			if (!headerPath.empty()) {
				if (!ScriptCompiler::loadFromHeader(headerPath)) {
					std::cerr << "[ProjectSerializer] Failed to load script from header: " << headerPath << "\n";
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

		if (ej.contains("scripts") && ej["scripts"].is_array()) {
			for (const auto& sj : ej["scripts"]) {
				loadScriptEntry(sj);
			}
		}

		// --- Collider ---
		if (ej.contains("collider")) {
			const auto& cj = ej["collider"];
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
	}

	for (const auto& [childId, parentId] : pendingParentByChildId) {
		Entity* child = scene->findEntityById(childId);
		Entity* parent = scene->findEntityById(parentId);
		if (child && parent) {
			child->setParent(parent, false);
		}
	}

	scene->clearDirty();
	std::cout << "[ProjectSerializer] Loaded scene from: " << filePath << "\n";
	return true;
}
