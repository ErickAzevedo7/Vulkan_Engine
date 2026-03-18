#pragma once
#include "engine_api/EngineExport.h"

// Behaviour.h — the ONLY header a game script needs.
//
//   #include "Behaviour.h"
//
//   class MyScript : public Behaviour {
//       SCRIPT(MyScript);
//   public:
//       void onUpdate(float dt) override {
//           auto p = getPosition();
//           p.x += 1.0f * dt;
//           setPosition(p);
//       }
//   };

// clang-format off
#include "components/ScriptComponent.h"  // needed: Behaviour inherits from it
#include "managers/ScriptRegistry.h"      // provides scriptRegistry_register
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
// clang-format on

// ---------------------------------------------------------------------------
// Behaviour
//
// The sole ENGINE_API class scripts see.
//
// Lifecycle overrides:
//   onStart()       — called once when Play begins
//   onUpdate(dt)    — called every frame
//   onStop()        — called once when Play ends
//
// Transform helpers (no Transform.h needed in your script):
//   getPosition() / setPosition(v)
//   getRotation() / setRotation(q)
//   getScale()    / setScale(v)
// ---------------------------------------------------------------------------
class ENGINE_API Behaviour : public ScriptComponent {
protected:
	explicit Behaviour(const char* name);

public:
	virtual ~Behaviour() = default;

	// Transform property accessors — implemented in Behaviour.cpp
	glm::vec3 getPosition() const;
	void setPosition(glm::vec3 v);

	glm::quat getRotation() const;
	void setRotation(glm::quat q);

	glm::vec3 getScale() const;
	void setScale(glm::vec3 v);
};

// ---------------------------------------------------------------------------
// ScriptAutoRegistrar<T>  +  SCRIPT(ClassName)
// ---------------------------------------------------------------------------
template<typename T> struct ScriptAutoRegistrar {
	explicit ScriptAutoRegistrar(const char* name) {
		scriptRegistry_register(name, [] { return static_cast<ScriptComponent*>(new T()); });
	}
};

// SCRIPT(ClassName) — place once inside your Behaviour subclass.
// Generates the default constructor and the static self-registrar.
#define SCRIPT(ClassName)                                                                                              \
public:                                                                                                                \
	ClassName() : Behaviour(#ClassName) {                                                                              \
	}                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
	static inline ::ScriptAutoRegistrar<ClassName> _autoReg_##ClassName {                                              \
		#ClassName                                                                                                     \
	}
