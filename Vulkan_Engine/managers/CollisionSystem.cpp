#include "managers/CollisionSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "components/ColliderComponent.h"
#include "components/Transform.h"
#include "Entity.h"
#include "Scene.h"
#include "core/events/EventBus.h"
#include "core/events/CollisionEvents.h"

#include "glm/common.hpp"

namespace {

struct RuntimeCollider {
	Entity* entity = nullptr;
	ColliderComponent* collider = nullptr;
	Transform* transform = nullptr;
	glm::vec3 worldMin = glm::vec3(0.0f);
	glm::vec3 worldMax = glm::vec3(0.0f);
	glm::vec3 worldCenter = glm::vec3(0.0f);
	glm::vec3 halfExtents = glm::vec3(0.5f);
};

bool intersects(const RuntimeCollider& a, const RuntimeCollider& b) {
	return (a.worldMin.x <= b.worldMax.x && a.worldMax.x >= b.worldMin.x) &&
		(a.worldMin.y <= b.worldMax.y && a.worldMax.y >= b.worldMin.y) &&
		(a.worldMin.z <= b.worldMax.z && a.worldMax.z >= b.worldMin.z);
}

} // namespace

void CollisionSystem::reset() {
	activePairs.clear();
}

void CollisionSystem::update(Scene& scene, Core::EventBus* eventBus) {
	std::vector<RuntimeCollider> colliders;
	auto* entities = scene.getEntities();
	if (!entities) {
		activePairs.clear();
		return;
	}

	colliders.reserve(entities->size());
	for (const auto& entityPtr : *entities) {
		if (!entityPtr) {
			continue;
		}

		Entity* entity = entityPtr.get();
		auto* collider = entity->getComponent<ColliderComponent>();
		auto* transform = entity->getComponent<Transform>();
		if (!collider || !transform || !collider->enabled) {
			continue;
		}

		RuntimeCollider runtimeCollider;
		runtimeCollider.entity = entity;
		runtimeCollider.collider = collider;
		runtimeCollider.transform = transform;

		const glm::vec3 worldCenter = transform->getWorldPosition() + collider->center;
		const glm::vec3 worldScale = transform->getWorldScale();
		const glm::vec3 absScale = glm::abs(worldScale);
		const glm::vec3 halfExtents = glm::max(collider->size * absScale * 0.5f, glm::vec3(0.0001f));
		runtimeCollider.worldCenter = worldCenter;
		runtimeCollider.halfExtents = halfExtents;
		runtimeCollider.worldMin = worldCenter - halfExtents;
		runtimeCollider.worldMax = worldCenter + halfExtents;

		colliders.push_back(runtimeCollider);
	}

	std::unordered_set<PairKey> framePairs;
	for (size_t i = 0; i < colliders.size(); ++i) {
		for (size_t j = i + 1; j < colliders.size(); ++j) {
			const RuntimeCollider& a = colliders[i];
			const RuntimeCollider& b = colliders[j];
			if (!intersects(a, b)) {
				continue;
			}

			const PairKey key = makePairKey(a.entity->getID(), b.entity->getID());
			framePairs.insert(key);

			if (!(a.collider->isTrigger || b.collider->isTrigger)) {
				if (a.collider->isStatic && b.collider->isStatic) {
					continue;
				}

				const glm::vec3 delta = a.worldCenter - b.worldCenter;
				const glm::vec3 overlap = (a.halfExtents + b.halfExtents) - glm::abs(delta);
				if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
					continue;
				}

				glm::vec3 pushOut(0.0f);
				if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
					pushOut.x = (delta.x < 0.0f) ? -overlap.x : overlap.x;
				} else if (overlap.y <= overlap.z) {
					pushOut.y = (delta.y < 0.0f) ? -overlap.y : overlap.y;
				} else {
					pushOut.z = (delta.z < 0.0f) ? -overlap.z : overlap.z;
				}

				glm::vec3 moveA(0.0f);
				glm::vec3 moveB(0.0f);
				if (a.collider->isStatic) {
					moveB = -pushOut;
				} else if (b.collider->isStatic) {
					moveA = pushOut;
				} else {
					moveA = pushOut * 0.5f;
					moveB = -pushOut * 0.5f;
				}

				if (colliders[i].transform) {
					colliders[i].transform->setWorldPosition(colliders[i].transform->getWorldPosition() + moveA);
					colliders[i].worldCenter += moveA;
					colliders[i].worldMin += moveA;
					colliders[i].worldMax += moveA;
				}
				if (colliders[j].transform) {
					colliders[j].transform->setWorldPosition(colliders[j].transform->getWorldPosition() + moveB);
					colliders[j].worldCenter += moveB;
					colliders[j].worldMin += moveB;
					colliders[j].worldMax += moveB;
				}
				continue;
			}

			const bool existedLastFrame = (activePairs.find(key) != activePairs.end());
			if (eventBus) {
				if (existedLastFrame) {
					Core::TriggerStayEvent e1(a.entity->getID(), b.entity->getID());
					Core::TriggerStayEvent e2(b.entity->getID(), a.entity->getID());
					eventBus->publish(e1);
					eventBus->publish(e2);
				} else {
					Core::TriggerEnterEvent e1(a.entity->getID(), b.entity->getID());
					Core::TriggerEnterEvent e2(b.entity->getID(), a.entity->getID());
					eventBus->publish(e1);
					eventBus->publish(e2);
				}
			}
		}
	}

	for (const PairKey key : activePairs) {
		if (framePairs.find(key) != framePairs.end()) {
			continue;
		}

		const uint32_t a = static_cast<uint32_t>(key >> 32);
		const uint32_t b = static_cast<uint32_t>(key & 0xffffffffULL);
		
		if (eventBus) {
			Core::TriggerExitEvent e1(a, b);
			Core::TriggerExitEvent e2(b, a);
			eventBus->publish(e1);
			eventBus->publish(e2);
		}
	}

	activePairs = std::move(framePairs);
}

CollisionSystem::PairKey CollisionSystem::makePairKey(uint32_t a, uint32_t b) {
	if (a > b) {
		std::swap(a, b);
	}
	return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}
