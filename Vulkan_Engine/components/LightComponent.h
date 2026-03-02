#pragma once

#include "Component.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"

// Forward declarations
class Entity;

enum class LightType { Directional = 0, Point = 1, Spot = 2 };

class LightComponent : public Component {
public:
	LightComponent(Entity* owner, LightType type = LightType::Point);
	~LightComponent();

	// Accessors
	LightType getType() const;
	void setType(LightType t);

	// Basic light properties (public for quick editor access)
	glm::vec3 color{1.0f, 1.0f, 1.0f};
	float intensity{1.0f};
	float range{50.0f}; // used by point/spot
	glm::vec3 direction{0.0f, -1.0f, 0.0f}; // used by directional/spot
	float innerConeAngle{glm::radians(12.5f)};
	float outerConeAngle{glm::radians(17.5f)};

	// Lighting contribution factors
	glm::vec3 ambient{0.15f, 0.15f, 0.15f};
	glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
	glm::vec3 specular{1.0f, 1.0f, 1.0f};

	// Attenuation factors for point/spot lights
	// attenuation = 1.0 / (Kc + Kl * distance + Kq * distance * distance)
	float attenuationKc{1.0f}; // constant term
	float attenuationKl{0.09f}; // linear term
	float attenuationKq{0.032f}; // quadratic term

	// Lighting model selection
	bool useBlinnPhong{true}; // true = Blinn-Phong, false = Phong

	Entity* getOwner() const {
		return owner;
	}

	// Uniform layout for GPU upload (aligned)
	// Uniform layout for GPU upload (aligned to std140)
	struct alignas(16) LightUniform {
		glm::vec4 colorIntensity; // rgb=color, a=intensity
		glm::vec4 direction; // xyz=direction, w=pad
		glm::vec4 positionType; // xyz=position, w=type (0=directional,1=point,2=spot)
		glm::vec4 ambient; // rgb=ambient, w=pad
		glm::vec4 diffuse; // rgb=diffuse, w=pad
		glm::vec4 specular; // rgb=specular, w=pad
		float attenuationKc;
		float attenuationKl;
		float attenuationKq;
		float cutOff; // inner cone angle (cosine)
		float outerCutOff; // outer cone angle (cosine)
		int useBlinnPhong; // 1 = Blinn-Phong, 0 = Phong
		float far_plane; // point light shadow far plane
	};

	LightUniform getLightUniform() const;

private:
	Entity* owner{nullptr};
	LightType type{LightType::Point};
};
