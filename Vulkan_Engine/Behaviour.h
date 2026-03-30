#pragma once
#include "engine_api/EngineExport.h"

#include "components/ScriptComponent.h"
#include "managers/ScriptRegistry.h"
#include "ExposedVariable.h"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

template<typename T> struct ScriptAutoRegistrar {
	explicit ScriptAutoRegistrar(const char* name) {
		scriptRegistry_register(name, [] { return static_cast<ScriptComponent*>(new T()); });
	}
};

class ENGINE_API Behaviour : public ScriptComponent {
protected:
	explicit Behaviour(const char* name);

public:
	virtual ~Behaviour() = default;

	// Physics callbacks
	virtual void onTriggerEnter(uint32_t otherEntityId) { (void)otherEntityId; }
	virtual void onTriggerStay(uint32_t otherEntityId) { (void)otherEntityId; }
	virtual void onTriggerExit(uint32_t otherEntityId) { (void)otherEntityId; }

	glm::vec3 getPosition() const;
	void setPosition(glm::vec3 v);

	glm::quat getRotation() const;
	void setRotation(glm::quat q);

	glm::vec3 getScale() const;
	void setScale(glm::vec3 v);

	bool keyDown(int keycode) const;
	bool keyPressed(int keycode) const;
	bool keyReleased(int keycode) const;

	bool mouseDown(int button) const;
	bool mousePressed(int button) const;
	bool mouseReleased(int button) const;

	glm::vec2 mousePosition() const;
	glm::vec2 mouseDelta() const;
	glm::vec2 scrollDelta() const;
};

#define SCRIPT(ClassName)                                                                                              \
public:                                                                                                                \
	ClassName() : Behaviour(#ClassName) {                                                                              \
	}                                                                                                                  \
                                                                                                                        \
private:                                                                                                               \
	static inline ::ScriptAutoRegistrar<ClassName> _autoReg_##ClassName {                                              \
		#ClassName                                                                                                     \
	}

#define INSPECT(className, name, defaultVal)                                                                            \
public:                                                                                                                \
	float name = defaultVal;                                                                                             \
                                                                                                                        \
private:                                                                                                               \
	static inline const ::InspectorPropertyRegistrar CONCAT(_inspect_reg_, __LINE__) {                                    \
		#className, #name, InspectorPropertyType::Float, &className::name                                                \
	}

#define INSPECT_INT(className, name, defaultVal)                                                                        \
public:                                                                                                                \
	int name = defaultVal;                                                                                               \
                                                                                                                        \
private:                                                                                                               \
	static inline const ::InspectorPropertyRegistrar CONCAT(_inspect_reg_, __LINE__) {                                    \
		#className, #name, InspectorPropertyType::Int, &className::name                                                  \
	}

#define INSPECT_BOOL(className, name, defaultVal)                                                                      \
public:                                                                                                                \
	bool name = defaultVal;                                                                                              \
                                                                                                                        \
private:                                                                                                               \
	static inline const ::InspectorPropertyRegistrar CONCAT(_inspect_reg_, __LINE__) {                                    \
		#className, #name, InspectorPropertyType::Bool, &className::name                                                 \
	}

#define INSPECT_VEC3(className, name, x, y, z)                                                                        \
public:                                                                                                                \
	glm::vec3 name = glm::vec3(x, y, z);                                                                               \
                                                                                                                        \
private:                                                                                                               \
	static inline const ::InspectorPropertyRegistrar CONCAT(_inspect_reg_, __LINE__) {                                    \
		#className, #name, InspectorPropertyType::Vec3, &className::name                                                 \
	}

#define INSPECT_VEC4(className, name, x, y, z, w)                                                                    \
public:                                                                                                                \
	glm::vec4 name = glm::vec4(x, y, z, w);                                                                            \
                                                                                                                        \
private:                                                                                                               \
	static inline const ::InspectorPropertyRegistrar CONCAT(_inspect_reg_, __LINE__) {                                    \
		#className, #name, InspectorPropertyType::Vec4, &className::name                                                 \
	}
