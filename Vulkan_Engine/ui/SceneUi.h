#pragma once

class ResourceContext;
class InspectorUi;

class SceneUi {
public:
	SceneUi(ResourceContext& resources, InspectorUi& inspector);
	void render();

	int selectedEntity;

private:
	ResourceContext& resources;
	InspectorUi& inspector;
};
