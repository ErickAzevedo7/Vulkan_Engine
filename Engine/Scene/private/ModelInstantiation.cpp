#include "scene/ModelInstantiation.h"

#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/components/MeshComponent.h"

#include <assets/AssetManager.h>
#include <math/Transform.h>

#include <vector>

namespace VKE {

GameObject *InstantiateModel(Scene &scene, const AssetManager &assets, Handle<ModelData> model, GameObject *parent) {
	const ModelData *data = assets.GetModel(model);
	if (!data || data->Nodes.empty()) {
		return nullptr;
	}

	std::vector<GameObject *> objects;
	objects.reserve(data->Nodes.size());
	for (const ModelNode &node : data->Nodes) {
		GameObject *nodeParent = node.Parent >= 0 ? objects[static_cast<size_t>(node.Parent)] : parent;
		GameObject &object = scene.CreateObject(node.Name, nodeParent);
		object.GetTransform().SetLocal(Transform::Decompose(node.LocalTransform));
		if (node.Mesh.IsValid()) {
			object.AddComponent<MeshComponent>(node.Mesh);
		}
		objects.push_back(&object);
	}

	objects[0]->SetName(data->Name);
	return objects[0];
}

} // namespace VKE
