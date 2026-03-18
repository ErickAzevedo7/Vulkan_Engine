#pragma once

#include "engine_api/EngineExport.h"

class Entity;

class ENGINE_API Component {
public:
	Component();
	virtual ~Component();

	Entity* owner = nullptr;


	// Legacy update kept for backward compatibility
	void update();

	// Runtime lifecycle — called by the Scene when entering/leaving Play mode
	virtual void onStart() {}
	virtual void onUpdate(float dt) {}
	virtual void onStop() {}
};
