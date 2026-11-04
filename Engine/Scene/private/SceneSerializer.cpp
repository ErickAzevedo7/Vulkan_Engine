#include "scene/SceneSerializer.h"

#include "PropertyOps.h"
#include "scene/ComponentRegistry.h"
#include "scene/GameObject.h"
#include "scene/Prefab.h"
#include "scene/Properties.h"
#include "scene/Scene.h"
#include "scene/components/CameraComponent.h"
#include "scene/components/MissingComponent.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace VKE {

namespace {
// Parents before children, siblings in order, so a loader can build the hierarchy in one pass.
void CollectPreOrder(GameObject &object, std::vector<GameObject *> &out) {
	out.push_back(&object);
	for (GameObject *child : object.GetChildren()) {
		CollectPreOrder(*child, out);
	}
}

ObjectID ReadID(const YAML::Node &node, const char *key) {
	YAML::Node value = node[key];
	return value && value.IsScalar() ? ObjectID::Parse(value.Scalar()) : ObjectID();
}

void ReadActive(const YAML::Node &node, GameObject &object) {
	YAML::Node active = node["Active"];
	if (!active || !active.IsScalar()) {
		return;
	}
	try {
		object.SetActive(active.as<bool>());
	} catch (const YAML::Exception &) {
	}
}

// What a component whose type isn't there said in the file.
void WriteMissing(YAML::Emitter &out, const Component &component) {
	const std::string &text = static_cast<const MissingComponent &>(component).GetSavedText();
	YAML::Node node = YAML::Load(text);
	if (node.IsNull() || !node.IsDefined()) {
		node = YAML::Node(YAML::NodeType::Map);
	}
	out << node;
}

void WriteObject(YAML::Emitter &out, const GameObject &object, const AssetManager &assets, bool writeParent) {
	out << YAML::BeginMap;
	out << YAML::Key << "ID" << YAML::Value << YAML::DoubleQuoted << object.GetID().ToString();
	out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << object.GetName();
	if (GameObject *parent = object.GetParent(); parent && writeParent) {
		out << YAML::Key << "Parent" << YAML::Value << YAML::DoubleQuoted << parent->GetID().ToString();
	}
	out << YAML::Key << "Active" << YAML::Value << object.IsActive();

	out << YAML::Key << "Components" << YAML::Value << YAML::BeginMap;
	for (Component *component : object.GetComponents()) {
		const ComponentTypeInfo *type = ComponentRegistry::Find(component->GetTypeID());
		out << YAML::Key << type->Name << YAML::Value;
		if (type->Missing) {
			WriteMissing(out, *component);
			continue;
		}
		PropertyWriter writer(out, assets, object.GetName());
		writer.Write(*type->Properties, type->ToObject(component), type->ToObject(type->Defaults.get()));
	}
	out << YAML::EndMap;
	out << YAML::EndMap;
}

// An instance: its prefab and what differs from it, by the part's ID in the prefab.
void WriteInstance(YAML::Emitter &out, GameObject &root, PrefabLibrary &prefabs) {
	out << YAML::BeginMap;
	out << YAML::Key << "ID" << YAML::Value << YAML::DoubleQuoted << root.GetID().ToString();
	out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << root.GetName();
	out << YAML::Key << "Prefab" << YAML::Value << YAML::DoubleQuoted << root.GetPrefab().ToString();
	if (GameObject *parent = root.GetParent()) {
		out << YAML::Key << "Parent" << YAML::Value << YAML::DoubleQuoted << parent->GetID().ToString();
	}

	std::vector<GameObject *> parts;
	CollectPreOrder(root, parts);
	bool opened = false;
	for (GameObject *part : parts) {
		GameObject *pristine = prefabs.GetPart(*part);
		if (!pristine) {
			continue;
		}

		std::vector<std::pair<Component *, Component *>> changed;
		for (Component *component : part->GetComponents()) {
			const ComponentTypeInfo *type = ComponentRegistry::Find(component->GetTypeID());
			Component *original = pristine->GetComponent(component->GetTypeID());
			if (!original) {
				continue;
			}
			bool differs = type->Missing
					? static_cast<MissingComponent *>(component)->GetSavedText() !=
							static_cast<MissingComponent *>(original)->GetSavedText()
					: !AreEqual(*type->Properties, type->ToObject(component), type->ToObject(original));
			if (differs) {
				changed.emplace_back(component, original);
			}
		}
		bool renamed = part != &root && part->GetName() != pristine->GetName();
		bool switched = part->IsActive() != pristine->IsActive();
		if (changed.empty() && !renamed && !switched) {
			continue;
		}

		if (!opened) {
			out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginSeq;
		}
		opened = true;
		out << YAML::BeginMap;
		out << YAML::Key << "Part" << YAML::Value << YAML::DoubleQuoted << part->GetPrefabPart().ToString();
		if (renamed) {
			out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << part->GetName();
		}
		if (switched) {
			out << YAML::Key << "Active" << YAML::Value << part->IsActive();
		}
		if (!changed.empty()) {
			out << YAML::Key << "Components" << YAML::Value << YAML::BeginMap;
			for (auto [component, original] : changed) {
				const ComponentTypeInfo *type = ComponentRegistry::Find(component->GetTypeID());
				out << YAML::Key << type->Name << YAML::Value;
				if (type->Missing) {
					WriteMissing(out, *component);
					continue;
				}
				PropertyWriter writer(out, prefabs.GetAssets(), part->GetName());
				writer.Write(*type->Properties, type->ToObject(component), type->ToObject(original));
			}
			out << YAML::EndMap;
		}
		out << YAML::EndMap;
	}
	if (opened) {
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;
}

void ReadOverrides(const YAML::Node &overrides, GameObject &root, AssetManager &assets) {
	if (!overrides || !overrides.IsSequence()) {
		return;
	}
	std::vector<GameObject *> parts;
	CollectPreOrder(root, parts);

	for (const YAML::Node &node : overrides) {
		if (!node.IsMap()) {
			continue;
		}
		ObjectID partID = ReadID(node, "Part");
		auto found = std::find_if(
				parts.begin(), parts.end(), [&](GameObject *part) { return part->GetPrefabPart() == partID; });
		if (found == parts.end()) {
			VKE_CORE_WARN("SceneSerializer: an override of '{}' is for a part its prefab doesn't have anymore "
						  "and is ignored",
					root.GetName());
			continue;
		}

		GameObject &object = **found;
		if (node["Name"] && node["Name"].IsScalar()) {
			object.SetName(node["Name"].Scalar());
		}
		ReadActive(node, object);

		YAML::Node components = node["Components"];
		if (!components || !components.IsMap()) {
			continue;
		}
		for (const auto &item : components) {
			std::string typeName = item.first.Scalar();
			const ComponentTypeInfo *type = ComponentRegistry::Find(typeName);
			Component *component = type ? object.GetComponent(type->ID) : nullptr;
			if (!component) {
				VKE_CORE_WARN("SceneSerializer: an override of '{}' is for a component '{}' it doesn't have "
							  "and is ignored",
						object.GetName(), typeName);
				continue;
			}
			if (type->Missing) {
				static_cast<MissingComponent *>(component)->SetSavedText(YAML::Dump(item.second));
				continue;
			}
			PropertyReader reader(assets, object.GetName());
			reader.Read(*type->Properties, item.second, type->ToObject(component), std::string(type->Name) + ".");
			component->ApplyPropertyChanges(assets);
		}
	}
}

bool WriteFile(const std::string &text, const std::filesystem::path &file) {
	std::ofstream stream(file, std::ios::binary | std::ios::trunc);
	stream << text;
	if (!stream) {
		VKE_CORE_ERROR("SceneSerializer: couldn't write '{}'", file.string());
		return false;
	}
	return true;
}

std::string ToFileText(const YAML::Emitter &out) {
	if (!out.good()) {
		VKE_CORE_ERROR("SceneSerializer: couldn't write the objects: {}", out.GetLastError());
		return {};
	}
	return std::string(out.c_str()) + '\n';
}
} // namespace

std::string SceneSerializer::ToText(const Scene &scene, PrefabLibrary &prefabs) {
	std::vector<GameObject *> objects;
	for (GameObject *root : scene.GetRootObjects()) {
		CollectPreOrder(*root, objects);
	}

	YAML::Emitter out;
	out << YAML::Comment("Written by the engine.");
	out << YAML::BeginMap;
	if (CameraComponent *camera = scene.GetMainCamera()) {
		out << YAML::Key << "MainCamera" << YAML::Value << YAML::DoubleQuoted << camera->GetOwner().GetID().ToString();
	}

	out << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;
	for (GameObject *object : objects) {
		// An instance whose prefab is gone is kept as plain objects.
		if (object->IsPrefabPart() && prefabs.GetRoot(object->GetPrefab())) {
			if (object->IsPrefabRoot()) {
				WriteInstance(out, *object, prefabs);
			}
		} else {
			WriteObject(out, *object, prefabs.GetAssets(), true);
		}
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return ToFileText(out);
}

bool SceneSerializer::Save(const Scene &scene, PrefabLibrary &prefabs, const std::filesystem::path &file) {
	std::string text = ToText(scene, prefabs);
	if (text.empty() || !WriteFile(text, file)) {
		return false;
	}
	VKE_CORE_INFO("SceneSerializer: saved {} objects to '{}'", scene.GetObjectCount(), file.string());
	return true;
}

bool SceneSerializer::Load(Scene &scene, PrefabLibrary &prefabs, const std::filesystem::path &file) {
	YAML::Node root;
	try {
		root = YAML::LoadFile(file.string());
	} catch (const YAML::Exception &error) {
		VKE_CORE_ERROR("SceneSerializer: '{}': {}", file.string(), error.what());
		return false;
	}
	return LoadObjects(scene, prefabs.GetAssets(), &prefabs, root, file.string());
}

bool SceneSerializer::LoadText(Scene &scene, PrefabLibrary &prefabs, const std::string &text, const std::string &name) {
	YAML::Node root;
	try {
		root = YAML::Load(text);
	} catch (const YAML::Exception &error) {
		VKE_CORE_ERROR("SceneSerializer: '{}': {}", name, error.what());
		return false;
	}
	return LoadObjects(scene, prefabs.GetAssets(), &prefabs, root, name);
}

std::string SceneSerializer::PrefabToText(GameObject &root, const AssetManager &assets) {
	std::vector<GameObject *> objects;
	CollectPreOrder(root, objects);

	YAML::Emitter out;
	out << YAML::Comment("Written by the engine.");
	out << YAML::BeginMap;
	out << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;
	for (const GameObject *object : objects) {
		WriteObject(out, *object, assets, object != &root);
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return ToFileText(out);
}

bool SceneSerializer::SavePrefab(GameObject &root, const AssetManager &assets, const std::filesystem::path &file) {
	std::string text = PrefabToText(root, assets);
	if (text.empty() || !WriteFile(text, file)) {
		return false;
	}
	VKE_CORE_INFO("SceneSerializer: saved the prefab '{}' to '{}'", root.GetName(), file.string());
	return true;
}

bool SceneSerializer::LoadPrefab(Scene &scene, AssetManager &assets, const std::filesystem::path &file) {
	YAML::Node root;
	try {
		root = YAML::LoadFile(file.string());
	} catch (const YAML::Exception &error) {
		VKE_CORE_ERROR("SceneSerializer: '{}': {}", file.string(), error.what());
		return false;
	}
	return LoadObjects(scene, assets, nullptr, root, file.string());
}

bool SceneSerializer::LoadPrefabText(
		Scene &scene, AssetManager &assets, const std::string &text, const std::string &name) {
	YAML::Node root;
	try {
		root = YAML::Load(text);
	} catch (const YAML::Exception &error) {
		VKE_CORE_ERROR("SceneSerializer: '{}': {}", name, error.what());
		return false;
	}
	return LoadObjects(scene, assets, nullptr, root, name);
}

std::string SceneSerializer::ComponentToText(Component &component, const AssetManager &assets) {
	const ComponentTypeInfo *type = ComponentRegistry::Find(component.GetTypeID());
	YAML::Emitter out;
	PropertyWriter writer(out, assets, component.GetOwner().GetName());
	writer.Write(*type->Properties, type->ToObject(&component), type->ToObject(type->Defaults.get()));
	return out.c_str();
}

void SceneSerializer::LoadComponentText(Component &component, AssetManager &assets, const std::string &text) {
	const ComponentTypeInfo *type = ComponentRegistry::Find(component.GetTypeID());
	PropertyReader reader(assets, component.GetOwner().GetName());
	reader.Read(*type->Properties, YAML::Load(text), type->ToObject(&component), std::string(type->Name) + ".");
	component.ApplyPropertyChanges(assets);
}

bool SceneSerializer::LoadObjects(Scene &scene, AssetManager &assets, PrefabLibrary *prefabs, const YAML::Node &root,
		const std::string &sourceName) {
	YAML::Node objectNodes = root["Objects"];
	if (!root.IsMap() || !objectNodes || !objectNodes.IsSequence()) {
		VKE_CORE_ERROR("SceneSerializer: '{}' is not a scene file", sourceName);
		return false;
	}

	bool hadMainCamera = scene.GetMainCamera() != nullptr;

	// First every object, so parents can be found by ID whatever their place in the file.
	struct Loaded {
		YAML::Node Node;
		GameObject *Object = nullptr;
		bool IsInstance = false;
	};
	std::vector<Loaded> loaded;
	std::unordered_map<ObjectID, GameObject *> byID;
	std::unordered_set<ObjectID> skipped;
	for (const YAML::Node &node : objectNodes) {
		if (!node.IsMap()) {
			continue;
		}
		ObjectID id = ReadID(node, "ID");
		std::string name = node["Name"] && node["Name"].IsScalar() ? node["Name"].Scalar() : "GameObject";

		ObjectID parent = ReadID(node, "Parent");
		if (skipped.count(parent)) {
			skipped.insert(id);
			continue;
		}
		if (!id.IsValid() || byID.count(id) || scene.FindByID(id)) {
			VKE_CORE_WARN(
					"SceneSerializer: '{}' in '{}' has a missing or repeated ID and is skipped", name, sourceName);
			skipped.insert(id);
			continue;
		}

		YAML::Node prefabNode = node["Prefab"];
		if (prefabNode && prefabNode.IsScalar()) {
			GameObject *instance = nullptr;
			if (prefabs) {
				instance = prefabs->Instantiate(scene, AssetID::Parse(prefabNode.Scalar()), nullptr, id);
			} else {
				VKE_CORE_WARN("SceneSerializer: prefabs can't go inside prefabs; '{}' in '{}' is left empty", name,
						sourceName);
			}
			if (instance) {
				instance->SetName(name);
				std::vector<GameObject *> parts;
				CollectPreOrder(*instance, parts);
				for (GameObject *part : parts) {
					byID[part->GetID()] = part;
				}
				loaded.push_back({ node, instance, true });
				continue;
			}
			if (prefabs) {
				VKE_CORE_WARN("SceneSerializer: '{}' in '{}' uses a prefab that can't be loaded and is left empty",
						name, sourceName);
			}
		}

		GameObject &object = scene.CreateObject(name, id);
		byID[id] = &object;
		loaded.push_back({ node, &object, false });
	}

	// Then the hierarchy, in file order so siblings keep their order.
	for (const Loaded &entry : loaded) {
		ObjectID parentID = ReadID(entry.Node, "Parent");
		if (!parentID.IsValid()) {
			continue;
		}
		auto parent = byID.find(parentID);
		if (parent == byID.end()) {
			VKE_CORE_WARN("SceneSerializer: '{}' has a parent that isn't in the file", entry.Object->GetName());
			continue;
		}
		entry.Object->SetParent(parent->second, false);
	}

	for (const Loaded &entry : loaded) {
		GameObject &object = *entry.Object;
		if (entry.IsInstance) {
			ReadOverrides(entry.Node["Overrides"], object, assets);
			continue;
		}
		ReadActive(entry.Node, object);

		YAML::Node components = entry.Node["Components"];
		if (!components || !components.IsMap()) {
			continue;
		}
		for (const auto &item : components) {
			std::string typeName = item.first.Scalar();
			const ComponentTypeInfo *type = &ComponentRegistry::FindOrAddMissing(typeName);
			Component *component = type->ID == ComponentRegistry::GetID<TransformComponent>()
					? &object.GetTransform()
					: object.AddComponent(type->ID);
			if (!component) {
				continue;
			}
			if (type->Missing) {
				VKE_CORE_WARN("SceneSerializer: '{}' has a component of unknown type '{}', kept as it is until "
							  "the type is back",
						object.GetName(), typeName);
				static_cast<MissingComponent *>(component)->SetSavedText(YAML::Dump(item.second));
				continue;
			}

			PropertyReader reader(assets, object.GetName());
			reader.Read(*type->Properties, item.second, type->ToObject(component), std::string(type->Name) + ".");
			component->ApplyPropertyChanges(assets);
		}
	}

	// A scene that had no main camera takes the file's.
	if (!hadMainCamera) {
		if (auto it = byID.find(ReadID(root, "MainCamera")); it != byID.end()) {
			if (auto *camera = it->second->GetComponent<CameraComponent>()) {
				scene.SetMainCamera(camera);
			}
		}
	}

	VKE_CORE_INFO("SceneSerializer: loaded {} objects from '{}'", loaded.size(), sourceName);
	return true;
}

} // namespace VKE
