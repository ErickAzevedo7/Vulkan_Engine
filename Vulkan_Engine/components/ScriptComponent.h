#pragma once

#include <string>

#include "Component.h"
#include "engine_api/EngineExport.h"

// ScriptComponent -------------------------------------------------------
// Base class for all native C++ game scripts.
// Subclass this and override onStart / onUpdate / onStop.
// The Scene calls these automatically during Play mode.
// -----------------------------------------------------------------------
#include <iostream>

class ENGINE_API ScriptComponent : public Component {
public:
	// Name identifying this script type (used for serialization)
	std::string scriptName;

	// Absolute path to the .h file that defines this script
	std::string headerPath;

	// Set to false to pause the script without removing it
	bool enabled = true;

	explicit ScriptComponent(const std::string& name = "MyScript") : scriptName(name) {
	}
	~ScriptComponent() override = default;

	// Lifecycle — override these in your subclass
	virtual void onStart() override {
		if (!enabled)
			return;
		std::cout << "[Script: " << scriptName << "] onStart\n";
	}
	virtual void onUpdate(float dt) override {
		(void)dt;
	}
	virtual void onStop() override {
		if (!enabled)
			return;
		std::cout << "[Script: " << scriptName << "] onStop\n";
	}
};
