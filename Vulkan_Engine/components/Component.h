#pragma once

#include "engine_api/EngineExport.h"

class Entity;

class ENGINE_API Component {
public:
	Component();
	virtual ~Component();

	Entity* owner = nullptr;

	virtual void onStart() {
	}
	virtual void onUpdate(float dt) {
	}
	virtual void onStop() {
	}
};
