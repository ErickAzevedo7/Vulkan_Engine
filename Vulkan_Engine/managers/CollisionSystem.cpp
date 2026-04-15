#include "managers/CollisionSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "Behaviour.h"
#include "components/ColliderComponent.h"
#include "components/MeshComponent.h"
#include "components/StaticMeshColliderComponent.h"
#include "components/Transform.h"
#include "core/events/CollisionEvents.h"
#include "core/events/EventBus.h"
#include "Entity.h"
#include "managers/MeshManager.h"
#include "Scene.h"

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"

namespace {

// Small skin to prevent the AABB from sitting exactly on the surface.
constexpr float kSkinWidth = 0.001f;

struct Triangle {
	glm::vec3 a = glm::vec3(0.0f);
	glm::vec3 b = glm::vec3(0.0f);
	glm::vec3 c = glm::vec3(0.0f);
};

struct RuntimeCollider {
	Entity* entity = nullptr;
	Transform* transform = nullptr;
	bool isTrigger = false;
	bool isStatic = false;
	bool isBoxCollider = false;
	glm::vec3 worldMin = glm::vec3(0.0f);
	glm::vec3 worldMax = glm::vec3(0.0f);
	glm::vec3 worldCenter = glm::vec3(0.0f);
	glm::vec3 halfExtents = glm::vec3(0.5f);
	std::vector<Triangle> triangles;
};

// ── Broad-phase AABB overlap ──────────────────────────────────────────────────

bool intersects(const RuntimeCollider& a, const RuntimeCollider& b) {
	return (a.worldMin.x <= b.worldMax.x && a.worldMax.x >= b.worldMin.x) &&
		   (a.worldMin.y <= b.worldMax.y && a.worldMax.y >= b.worldMin.y) &&
		   (a.worldMin.z <= b.worldMax.z && a.worldMax.z >= b.worldMin.z);
}

// ── Triangle-soup helpers (used for trigger overlap detection) ──────────────

bool overlaps1D(float minA, float maxA, float minB, float maxB) {
	return !(maxA < minB || maxB < minA);
}

void projectTriangle(const Triangle& triangle, const glm::vec3& axis, float& outMin, float& outMax) {
	const float p0 = glm::dot(triangle.a, axis);
	const float p1 = glm::dot(triangle.b, axis);
	const float p2 = glm::dot(triangle.c, axis);
	outMin = std::min(p0, std::min(p1, p2));
	outMax = std::max(p0, std::max(p1, p2));
}

bool satAxisOverlap(const Triangle& a, const Triangle& b, const glm::vec3& axis) {
	constexpr float kEpsilon = 1e-6f;
	const float axisLenSq = glm::dot(axis, axis);
	if (axisLenSq <= kEpsilon) {
		return true;
	}

	const glm::vec3 normalizedAxis = axis * (1.0f / std::sqrt(axisLenSq));
	float minA = 0.0f;
	float maxA = 0.0f;
	float minB = 0.0f;
	float maxB = 0.0f;
	projectTriangle(a, normalizedAxis, minA, maxA);
	projectTriangle(b, normalizedAxis, minB, maxB);
	return overlaps1D(minA, maxA, minB, maxB);
}

glm::vec2 projectTo2D(const glm::vec3& p, int dropAxis) {
	if (dropAxis == 0) {
		return glm::vec2(p.y, p.z);
	}
	if (dropAxis == 1) {
		return glm::vec2(p.x, p.z);
	}
	return glm::vec2(p.x, p.y);
}

float cross2D(const glm::vec2& a, const glm::vec2& b) {
	return (a.x * b.y) - (a.y * b.x);
}

bool pointInTriangle2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
	constexpr float kEpsilon = 1e-6f;
	const glm::vec2 ab = b - a;
	const glm::vec2 bc = c - b;
	const glm::vec2 ca = a - c;
	const glm::vec2 ap = p - a;
	const glm::vec2 bp = p - b;
	const glm::vec2 cp = p - c;

	const float c1 = cross2D(ab, ap);
	const float c2 = cross2D(bc, bp);
	const float c3 = cross2D(ca, cp);

	const bool hasNeg = (c1 < -kEpsilon) || (c2 < -kEpsilon) || (c3 < -kEpsilon);
	const bool hasPos = (c1 > kEpsilon) || (c2 > kEpsilon) || (c3 > kEpsilon);
	return !(hasNeg && hasPos);
}

bool segmentsIntersect2D(const glm::vec2& a0, const glm::vec2& a1, const glm::vec2& b0, const glm::vec2& b1) {
	constexpr float kEpsilon = 1e-6f;
	const glm::vec2 r = a1 - a0;
	const glm::vec2 s = b1 - b0;
	const float rxs = cross2D(r, s);
	const float qpxr = cross2D(b0 - a0, r);

	if (std::abs(rxs) <= kEpsilon && std::abs(qpxr) <= kEpsilon) {
		const float rr = glm::dot(r, r);
		if (rr <= kEpsilon) {
			const glm::vec2 d = a0 - b0;
			return glm::dot(d, d) <= kEpsilon;
		}
		const float t0 = glm::dot(b0 - a0, r) / rr;
		const float t1 = glm::dot(b1 - a0, r) / rr;
		const float tMin = std::min(t0, t1);
		const float tMax = std::max(t0, t1);
		return !(tMax < 0.0f || tMin > 1.0f);
	}

	if (std::abs(rxs) <= kEpsilon) {
		return false;
	}

	const float t = cross2D(b0 - a0, s) / rxs;
	const float u = cross2D(b0 - a0, r) / rxs;
	return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

bool coplanarTrianglesIntersect(const Triangle& a, const Triangle& b, const glm::vec3& normal) {
	const glm::vec3 n = glm::abs(normal);
	int dropAxis = 2;
	if (n.x > n.y && n.x > n.z) {
		dropAxis = 0;
	} else if (n.y > n.z) {
		dropAxis = 1;
	}

	const glm::vec2 a2[3] = {projectTo2D(a.a, dropAxis), projectTo2D(a.b, dropAxis), projectTo2D(a.c, dropAxis)};
	const glm::vec2 b2[3] = {projectTo2D(b.a, dropAxis), projectTo2D(b.b, dropAxis), projectTo2D(b.c, dropAxis)};

	for (int i = 0; i < 3; ++i) {
		const int in = (i + 1) % 3;
		for (int j = 0; j < 3; ++j) {
			const int jn = (j + 1) % 3;
			if (segmentsIntersect2D(a2[i], a2[in], b2[j], b2[jn])) {
				return true;
			}
		}
	}

	if (pointInTriangle2D(a2[0], b2[0], b2[1], b2[2])) {
		return true;
	}
	if (pointInTriangle2D(b2[0], a2[0], a2[1], a2[2])) {
		return true;
	}

	return false;
}

bool trianglesIntersect(const Triangle& a, const Triangle& b) {
	constexpr float kEpsilon = 1e-6f;
	const glm::vec3 aEdges[3] = {a.b - a.a, a.c - a.b, a.a - a.c};
	const glm::vec3 bEdges[3] = {b.b - b.a, b.c - b.b, b.a - b.c};

	const glm::vec3 aNormal = glm::cross(aEdges[0], aEdges[1]);
	const glm::vec3 bNormal = glm::cross(bEdges[0], bEdges[1]);

	if (!satAxisOverlap(a, b, aNormal) || !satAxisOverlap(a, b, bNormal)) {
		return false;
	}

	for (const glm::vec3& aEdge : aEdges) {
		for (const glm::vec3& bEdge : bEdges) {
			if (!satAxisOverlap(a, b, glm::cross(aEdge, bEdge))) {
				return false;
			}
		}
	}

	const float aNormalLenSq = glm::dot(aNormal, aNormal);
	const float bNormalLenSq = glm::dot(bNormal, bNormal);
	if (aNormalLenSq <= kEpsilon || bNormalLenSq <= kEpsilon) {
		return false;
	}

	const glm::vec3 nA = aNormal * (1.0f / std::sqrt(aNormalLenSq));
	const glm::vec3 nB = bNormal * (1.0f / std::sqrt(bNormalLenSq));
	const float normalAlignment = std::abs(glm::dot(nA, nB));
	if (normalAlignment > 0.999f) {
		const float planeDistance = std::abs(glm::dot(nA, b.a - a.a));
		if (planeDistance <= 1e-4f) {
			return coplanarTrianglesIntersect(a, b, nA);
		}
	}

	return true;
}

void buildAabbTriangles(const glm::vec3& worldMin, const glm::vec3& worldMax, std::vector<Triangle>& outTriangles) {
	const glm::vec3 v[8] = {{worldMin.x, worldMin.y, worldMin.z},
							{worldMax.x, worldMin.y, worldMin.z},
							{worldMax.x, worldMax.y, worldMin.z},
							{worldMin.x, worldMax.y, worldMin.z},
							{worldMin.x, worldMin.y, worldMax.z},
							{worldMax.x, worldMin.y, worldMax.z},
							{worldMax.x, worldMax.y, worldMax.z},
							{worldMin.x, worldMax.y, worldMax.z}};

	const uint32_t idx[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 4, 7, 7, 3, 0,
							1, 5, 6, 6, 2, 1, 3, 2, 6, 6, 7, 3, 0, 1, 5, 5, 4, 0};

	outTriangles.clear();
	outTriangles.reserve(12);
	for (size_t i = 0; i < std::size(idx); i += 3) {
		outTriangles.push_back(Triangle{v[idx[i]], v[idx[i + 1]], v[idx[i + 2]]});
	}
}

bool triangleSoupIntersects(const RuntimeCollider& a, const RuntimeCollider& b) {
	for (const Triangle& ta : a.triangles) {
		for (const Triangle& tb : b.triangles) {
			if (trianglesIntersect(ta, tb)) {
				return true;
			}
		}
	}
	return false;
}

// ── Tri-vs-tri MTV (used for box-vs-box and mesh-vs-mesh fallback) ──

float projectionOverlap(float minA, float maxA, float minB, float maxB) {
	return std::min(maxA, maxB) - std::max(minA, minB);
}

bool aabbPairSeparation(const RuntimeCollider& a, const RuntimeCollider& b, glm::vec3& outMoveA, glm::vec3& outNormalA) {
	const glm::vec3 delta = a.worldCenter - b.worldCenter;
	const glm::vec3 overlap = (a.halfExtents + b.halfExtents) - glm::abs(delta);
	if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
		return false;
	}

	int axis = 0;
	float minOverlap = overlap.x;
	if (overlap.y < minOverlap) {
		minOverlap = overlap.y;
		axis = 1;
	}
	if (overlap.z < minOverlap) {
		minOverlap = overlap.z;
		axis = 2;
	}

	outMoveA = glm::vec3(0.0f);
	outMoveA[axis] = (delta[axis] >= 0.0f ? 1.0f : -1.0f) * (minOverlap + kSkinWidth);

	const float moveLen = glm::length(outMoveA);
	outNormalA = (moveLen > 1e-6f) ? outMoveA / moveLen : glm::vec3(0.0f, 1.0f, 0.0f);
	return true;
}

bool triangleFaceMtv(const Triangle& a, const Triangle& b, glm::vec3& outMtv) {
	constexpr float kEpsilon = 1e-6f;

	const glm::vec3 aEdges[3] = {a.b - a.a, a.c - a.b, a.a - a.c};
	const glm::vec3 aNormal = glm::cross(aEdges[0], aEdges[1]);
	const float aNormalLenSq = glm::dot(aNormal, aNormal);
	if (aNormalLenSq <= kEpsilon) {
		return false;
	}

	const glm::vec3 aNormalDir = aNormal * (1.0f / std::sqrt(aNormalLenSq));

	float aMin = 0.0f, aMax = 0.0f, bMin = 0.0f, bMax = 0.0f;
	projectTriangle(a, aNormalDir, aMin, aMax);
	projectTriangle(b, aNormalDir, bMin, bMax);

	const glm::vec3 aCenter = (a.a + a.b + a.c) * (1.0f / 3.0f);
	const glm::vec3 bCenter = (b.a + b.b + b.c) * (1.0f / 3.0f);
	const glm::vec3 delta = bCenter - aCenter;
	const float depth = projectionOverlap(aMin, aMax, bMin, bMax);

	if (depth <= kEpsilon) {
		return false;
	}

	const glm::vec3 direction = glm::dot(delta, aNormalDir) > 0.0f ? -aNormalDir : aNormalDir;
	outMtv = direction * depth;
	return true;
}

bool colliderPairMtv(const RuntimeCollider& a, const RuntimeCollider& b, glm::vec3& outMtv) {
	glm::vec3 bestMtv(0.0f);
	float bestLenSq = std::numeric_limits<float>::max();
	bool found = false;

	for (const Triangle& ta : a.triangles) {
		for (const Triangle& tb : b.triangles) {
			glm::vec3 triMtv(0.0f);
			if (!triangleFaceMtv(ta, tb, triMtv)) {
				continue;
			}
			const float lenSq = glm::dot(triMtv, triMtv);
			if (lenSq > 1e-10f && lenSq < bestLenSq) {
				bestLenSq = lenSq;
				bestMtv = triMtv;
				found = true;
			}
		}
	}

	if (!found) {
		return false;
	}

	outMtv = bestMtv;
	return true;
}

// ── AABB-vs-triangle resolution (single SAT path) ────────────────────────────

// Check if a triangle's AABB overlaps a given AABB (quick rejection).
bool triangleAabbOverlaps(const Triangle& tri, const glm::vec3& boxMin, const glm::vec3& boxMax) {
	const glm::vec3 triMin = glm::min(tri.a, glm::min(tri.b, tri.c));
	const glm::vec3 triMax = glm::max(tri.a, glm::max(tri.b, tri.c));
	return triMin.x <= boxMax.x && triMax.x >= boxMin.x && triMin.y <= boxMax.y && triMax.y >= boxMin.y &&
		   triMin.z <= boxMax.z && triMax.z >= boxMin.z;
}

void projectAabb(const glm::vec3& boxCenter,
				 const glm::vec3& halfExtents,
				 const glm::vec3& axis,
				 float& outMin,
				 float& outMax) {
	const float c = glm::dot(boxCenter, axis);
	const float r =
		halfExtents.x * std::abs(axis.x) + halfExtents.y * std::abs(axis.y) + halfExtents.z * std::abs(axis.z);
	outMin = c - r;
	outMax = c + r;
}

bool aabbVsTriangleSat(const glm::vec3& boxCenter,
					   const glm::vec3& halfExtents,
					   const Triangle& tri,
					   glm::vec3& outMtv,
					   glm::vec3& outNormal) {
	constexpr float kEpsilon = 1e-8f;

	const glm::vec3 triEdges[3] = {tri.b - tri.a, tri.c - tri.b, tri.a - tri.c};
	const glm::vec3 triNormalRaw = glm::cross(triEdges[0], triEdges[1]);
	const glm::vec3 triCenter = (tri.a + tri.b + tri.c) * (1.0f / 3.0f);
	const glm::vec3 delta = boxCenter - triCenter;

	std::vector<glm::vec3> axes;
	axes.reserve(13);
	axes.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
	axes.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
	axes.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
	axes.push_back(triNormalRaw);

	const glm::vec3 boxAxes[3] = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
	};
	for (const glm::vec3& e : triEdges) {
		for (const glm::vec3& a : boxAxes) {
			axes.push_back(glm::cross(e, a));
		}
	}

	float bestOverlap = std::numeric_limits<float>::max();
	glm::vec3 bestAxis(0.0f, 1.0f, 0.0f);

	for (const glm::vec3& axisRaw : axes) {
		const float lenSq = glm::dot(axisRaw, axisRaw);
		if (lenSq <= kEpsilon) {
			continue;
		}

		const glm::vec3 axis = axisRaw * (1.0f / std::sqrt(lenSq));

		float boxMin = 0.0f;
		float boxMax = 0.0f;
		float triMin = 0.0f;
		float triMax = 0.0f;
		projectAabb(boxCenter, halfExtents, axis, boxMin, boxMax);
		projectTriangle(tri, axis, triMin, triMax);

		float overlap = 0.0f;
		const float triThickness = triMax - triMin;
		if (triThickness <= 1e-6f) {
			// For zero-thickness projections (triangle plane axis), compute AABB-vs-plane overlap.
			const float planeDist = glm::dot(axis, boxCenter) - triMin;
			const float boxRadius =
				halfExtents.x * std::abs(axis.x) + halfExtents.y * std::abs(axis.y) + halfExtents.z * std::abs(axis.z);
			overlap = boxRadius - std::abs(planeDist);
		} else {
			overlap = projectionOverlap(boxMin, boxMax, triMin, triMax);
		}

		if (overlap < -1e-6f) {
			return false;
		}

		if (overlap < bestOverlap) {
			bestOverlap = overlap;
			bestAxis = (glm::dot(delta, axis) >= 0.0f) ? axis : -axis;
		}
	}

	// Keep sign convention consistent with existing resolver:
	// pushOut points toward penetration and the resolver applies -pushOut to the dynamic body.
	outMtv = -bestAxis * (bestOverlap + kSkinWidth);
	const float mtvLen = glm::length(outMtv);
	outNormal = (mtvLen > 1e-6f) ? outMtv / mtvLen : glm::vec3(0.0f, 1.0f, 0.0f);
	return true;
}

// Compute the AABB-vs-mesh-triangle MTV.
// Iterates all mesh triangles and accumulates the strongest push per axis.
// This keeps floor + wall blocking active at the same time.
bool aabbVsMeshMtv(const glm::vec3& boxCenter,
				   const glm::vec3& halfExtents,
				   const std::vector<Triangle>& meshTriangles,
				   glm::vec3& outMtv,
				   glm::vec3& outNormal) {
	const glm::vec3 boxMin = boxCenter - halfExtents;
	const glm::vec3 boxMax = boxCenter + halfExtents;

	glm::vec3 maxPush(0.0f);
	bool anyContact = false;

	for (const Triangle& tri : meshTriangles) {
		if (!triangleAabbOverlaps(tri, boxMin, boxMax)) {
			continue;
		}

		glm::vec3 triMtv(0.0f);
		glm::vec3 dummyNormal(0.0f, 1.0f, 0.0f);
		if (!aabbVsTriangleSat(boxCenter, halfExtents, tri, triMtv, dummyNormal)) {
			continue;
		}

		if (std::abs(triMtv.x) > std::abs(maxPush.x))
			maxPush.x = triMtv.x;
		if (std::abs(triMtv.y) > std::abs(maxPush.y))
			maxPush.y = triMtv.y;
		if (std::abs(triMtv.z) > std::abs(maxPush.z))
			maxPush.z = triMtv.z;
		anyContact = true;
	}

	if (!anyContact) {
		return false;
	}

	outMtv = maxPush;
	const float mtvLen = glm::length(maxPush);
	outNormal = (mtvLen > 1e-6f) ? maxPush / mtvLen : glm::vec3(0.0f, 1.0f, 0.0f);
	return true;
}

// Check if the AABB overlaps any mesh triangle (for trigger detection).
bool aabbOverlapsMesh(const glm::vec3& boxCenter,
					  const glm::vec3& halfExtents,
					  const std::vector<Triangle>& meshTriangles) {
	const glm::vec3 boxMin = boxCenter - halfExtents;
	const glm::vec3 boxMax = boxCenter + halfExtents;

	for (const Triangle& tri : meshTriangles) {
		if (!triangleAabbOverlaps(tri, boxMin, boxMax)) {
			continue;
		}

		glm::vec3 dummyMtv(0.0f);
		glm::vec3 dummyNormal(0.0f, 1.0f, 0.0f);
		if (aabbVsTriangleSat(boxCenter, halfExtents, tri, dummyMtv, dummyNormal)) {
			return true;
		}
	}
	return false;
}

// ── Translation helper ──────────────────────────────────────────────────────

void translateRuntimeCollider(RuntimeCollider& collider, const glm::vec3& delta) {
	if (collider.transform) {
		collider.transform->setWorldPosition(collider.transform->getWorldPosition() + delta);
	}
	collider.worldCenter += delta;
	collider.worldMin += delta;
	collider.worldMax += delta;
	for (Triangle& t : collider.triangles) {
		t.a += delta;
		t.b += delta;
		t.c += delta;
	}
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

	colliders.reserve(entities->size() * 2);
	for (const auto& entityPtr : *entities) {
		if (!entityPtr) {
			continue;
		}

		Entity* entity = entityPtr.get();
		auto* collider = entity->getComponent<ColliderComponent>();
		auto* staticMeshCollider = entity->getComponent<StaticMeshColliderComponent>();
		auto* meshComponent = entity->getComponent<MeshComponent>();
		auto* transform = entity->getComponent<Transform>();
		if (!transform) {
			continue;
		}

		if (collider && collider->enabled) {
			RuntimeCollider runtimeCollider;
			runtimeCollider.entity = entity;
			runtimeCollider.transform = transform;
			runtimeCollider.isTrigger = collider->isTrigger;
			runtimeCollider.isStatic = collider->isStatic;
			runtimeCollider.isBoxCollider = true;

			const glm::vec3 worldCenter = transform->getWorldPosition() + collider->center;
			const glm::vec3 worldScale = transform->getWorldScale();
			const glm::vec3 absScale = glm::abs(worldScale);
			const glm::vec3 halfExtents = glm::max(collider->size * absScale * 0.5f, glm::vec3(0.0001f));
			runtimeCollider.worldCenter = worldCenter;
			runtimeCollider.halfExtents = halfExtents;
			runtimeCollider.worldMin = worldCenter - halfExtents;
			runtimeCollider.worldMax = worldCenter + halfExtents;
			// Still build triangles for box-vs-box or mesh-vs-mesh fallback / triggers
			buildAabbTriangles(runtimeCollider.worldMin, runtimeCollider.worldMax, runtimeCollider.triangles);

			colliders.push_back(runtimeCollider);
		}

		if (staticMeshCollider && staticMeshCollider->enabled) {
			colliders.emplace_back();
			RuntimeCollider& runtimeCollider = colliders.back();
			runtimeCollider.entity = entity;
			runtimeCollider.transform = transform;
			runtimeCollider.isTrigger = staticMeshCollider->isTrigger;
			runtimeCollider.isStatic = true;
			runtimeCollider.isBoxCollider = false;

			const glm::vec3 worldPos = transform->getWorldPosition();
			const glm::quat worldRot = transform->getWorldRotation();
			const glm::vec3 worldScale = transform->getWorldScale();
			const glm::vec3 localCenter = staticMeshCollider->localCenter;
			const glm::vec3 localSize = glm::max(staticMeshCollider->localSize, glm::vec3(0.0001f));

			bool builtFromMesh = false;
			if (staticMeshCollider->useAttachedMeshBounds && meshComponent) {
				if (Mesh* mesh = meshComponent->GetMesh()) {
					if (!mesh->collisionVertices.empty() && mesh->collisionIndices.size() >= 3) {
						runtimeCollider.worldMin = glm::vec3(std::numeric_limits<float>::max());
						runtimeCollider.worldMax = glm::vec3(std::numeric_limits<float>::lowest());
						runtimeCollider.triangles.reserve(mesh->collisionIndices.size() / 3);

						for (size_t i = 0; i + 2 < mesh->collisionIndices.size(); i += 3) {
							const uint32_t i0 = mesh->collisionIndices[i];
							const uint32_t i1 = mesh->collisionIndices[i + 1];
							const uint32_t i2 = mesh->collisionIndices[i + 2];
							if (i0 >= mesh->collisionVertices.size() || i1 >= mesh->collisionVertices.size() ||
								i2 >= mesh->collisionVertices.size()) {
								continue;
							}

							auto toWorld = [&](const glm::vec3& localVertex) {
								const glm::vec3 shapedLocal = localCenter + (localVertex * localSize);
								const glm::vec3 scaled = shapedLocal * worldScale;
								return worldPos + (worldRot * scaled);
							};

							const glm::vec3 w0 = toWorld(mesh->collisionVertices[i0]);
							const glm::vec3 w1 = toWorld(mesh->collisionVertices[i1]);
							const glm::vec3 w2 = toWorld(mesh->collisionVertices[i2]);

							runtimeCollider.triangles.push_back(Triangle{w0, w1, w2});

							runtimeCollider.worldMin = glm::min(runtimeCollider.worldMin, w0);
							runtimeCollider.worldMin = glm::min(runtimeCollider.worldMin, w1);
							runtimeCollider.worldMin = glm::min(runtimeCollider.worldMin, w2);
							runtimeCollider.worldMax = glm::max(runtimeCollider.worldMax, w0);
							runtimeCollider.worldMax = glm::max(runtimeCollider.worldMax, w1);
							runtimeCollider.worldMax = glm::max(runtimeCollider.worldMax, w2);
						}

						if (!runtimeCollider.triangles.empty()) {
							builtFromMesh = true;
						}
					}
				}
			}

			if (!builtFromMesh) {
				const glm::vec3 worldCenter = worldPos + localCenter;
				const glm::vec3 absScale = glm::abs(worldScale);
				const glm::vec3 halfExtents = glm::max(localSize * absScale * 0.5f, glm::vec3(0.0001f));
				runtimeCollider.worldMin = worldCenter - halfExtents;
				runtimeCollider.worldMax = worldCenter + halfExtents;
				buildAabbTriangles(runtimeCollider.worldMin, runtimeCollider.worldMax, runtimeCollider.triangles);
			}

			runtimeCollider.worldCenter = (runtimeCollider.worldMin + runtimeCollider.worldMax) * 0.5f;
			runtimeCollider.halfExtents =
				glm::max((runtimeCollider.worldMax - runtimeCollider.worldMin) * 0.5f, glm::vec3(0.0001f));
		}
	}

	std::unordered_set<PairKey> framePairs;
	for (size_t i = 0; i < colliders.size(); ++i) {
		for (size_t j = i + 1; j < colliders.size(); ++j) {
			RuntimeCollider& a = colliders[i];
			RuntimeCollider& b = colliders[j];
			if (!intersects(a, b) || a.triangles.empty() || b.triangles.empty()) {
				continue;
			}

			const bool isBoxVsBox = a.isBoxCollider && b.isBoxCollider;

			// ── Determine if this is a box-vs-mesh pair ──────────────
			const bool isBoxVsMesh = (a.isBoxCollider && !b.isBoxCollider) || (!a.isBoxCollider && b.isBoxCollider);

			// ── Narrow-phase overlap test ─────────────────────────────
			bool overlapping = false;
			if (isBoxVsBox) {
				// Broad-phase is exact for AABB-vs-AABB.
				overlapping = true;
			} else if (isBoxVsMesh) {
				// Use the new AABB-vs-mesh overlap test.
				const RuntimeCollider& box = a.isBoxCollider ? a : b;
				const RuntimeCollider& mesh = a.isBoxCollider ? b : a;
				overlapping = aabbOverlapsMesh(box.worldCenter, box.halfExtents, mesh.triangles);
			} else {
				// box-vs-box or mesh-vs-mesh: use triangle soup.
				overlapping = triangleSoupIntersects(a, b);
			}

			if (!overlapping) {
				continue;
			}

			const PairKey key = makePairKey(a.entity->getID(), b.entity->getID());
			framePairs.insert(key);

			if (!(a.isTrigger || b.isTrigger)) {
				if (a.isStatic && b.isStatic) {
					continue;
				}

				// ── Solid collision resolution ───────────────────────
				glm::vec3 pushOut(0.0f);
				glm::vec3 contactNormal(0.0f, 1.0f, 0.0f);

				if (isBoxVsBox) {
					if (!aabbPairSeparation(a, b, pushOut, contactNormal)) {
						continue;
					}
				} else if (isBoxVsMesh) {
					// Use the new AABB-vs-mesh MTV (no triangle seam jitter).
					const RuntimeCollider& box = a.isBoxCollider ? a : b;
					const RuntimeCollider& mesh = a.isBoxCollider ? b : a;

					if (!aabbVsMeshMtv(box.worldCenter, box.halfExtents, mesh.triangles, pushOut, contactNormal)) {
						// Fallback to AABB overlap.
						const glm::vec3 delta = box.worldCenter - mesh.worldCenter;
						const glm::vec3 overlap = (box.halfExtents + mesh.halfExtents) - glm::abs(delta);
						if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
							continue;
						}
						if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
							pushOut.x = (delta.x < 0.0f) ? -overlap.x : overlap.x;
						} else if (overlap.y <= overlap.z) {
							pushOut.y = (delta.y < 0.0f) ? -overlap.y : overlap.y;
						} else {
							pushOut.z = (delta.z < 0.0f) ? -overlap.z : overlap.z;
						}
					}

					// For box-vs-mesh the push is computed from box perspective.
					// If 'a' is the box, pushOut is already correct (push A away from B).
					// If 'b' is the box, we need to negate for A's perspective.
					if (!a.isBoxCollider) {
						pushOut = -pushOut;
						contactNormal = -contactNormal;
					}
				} else {
					// box-vs-box or mesh-vs-mesh: old tri-pair MTV.
					if (!colliderPairMtv(a, b, pushOut)) {
						const glm::vec3 delta = a.worldCenter - b.worldCenter;
						const glm::vec3 overlap = (a.halfExtents + b.halfExtents) - glm::abs(delta);
						if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
							continue;
						}

						if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
							pushOut.x = (delta.x < 0.0f) ? -overlap.x : overlap.x;
						} else if (overlap.y <= overlap.z) {
							pushOut.y = (delta.y < 0.0f) ? -overlap.y : overlap.y;
						} else {
							pushOut.z = (delta.z < 0.0f) ? -overlap.z : overlap.z;
						}
					}
					const float pLen = glm::length(pushOut);
					contactNormal = (pLen > 1e-6f) ? pushOut / pLen : glm::vec3(0.0f, 1.0f, 0.0f);
				}

				glm::vec3 moveA(0.0f);
				glm::vec3 moveB(0.0f);

				if (isBoxVsBox) {
					if (a.isStatic) {
						moveB = -pushOut;
					} else if (b.isStatic) {
						moveA = pushOut;
					} else {
						moveA = pushOut * 0.5f;
						moveB = -pushOut * 0.5f;
					}
				} else {
					if (a.isStatic) {
						moveB = pushOut;
					} else if (b.isStatic) {
						moveA = -pushOut;
					} else {
						moveA = pushOut * 0.5f;
						moveB = -pushOut * 0.5f;
					}
				}

				translateRuntimeCollider(colliders[i], moveA);
				translateRuntimeCollider(colliders[j], moveB);

				for (auto* bh : a.entity->getComponents<Behaviour>()) {
					bh->onCollision(contactNormal);
				}
				for (auto* bh : b.entity->getComponents<Behaviour>()) {
					bh->onCollision(-contactNormal);
				}
				continue;
			}

			// ── Trigger events ───────────────────────────────────────
			// Compute a contact normal for trigger events.
			glm::vec3 triggerNormal(0.0f);
			if (isBoxVsMesh) {
				const RuntimeCollider& box = a.isBoxCollider ? a : b;
				const RuntimeCollider& mesh = a.isBoxCollider ? b : a;
				glm::vec3 dummyMtv(0.0f);
				aabbVsMeshMtv(box.worldCenter, box.halfExtents, mesh.triangles, dummyMtv, triggerNormal);
				if (!a.isBoxCollider) {
					triggerNormal = -triggerNormal;
				}
			} else {
				const glm::vec3 d = glm::normalize(b.worldCenter - a.worldCenter);
				triggerNormal = glm::length(d) > 1e-6f ? d : glm::vec3(0.0f, 1.0f, 0.0f);
			}

			const bool existedLastFrame = (activePairs.find(key) != activePairs.end());
			if (eventBus) {
				if (existedLastFrame) {
					Core::TriggerStayEvent e1(a.entity->getID(), b.entity->getID(), triggerNormal);
					Core::TriggerStayEvent e2(b.entity->getID(), a.entity->getID(), -triggerNormal);
					eventBus->publish(e1);
					eventBus->publish(e2);
				} else {
					Core::TriggerEnterEvent e1(a.entity->getID(), b.entity->getID(), triggerNormal);
					Core::TriggerEnterEvent e2(b.entity->getID(), a.entity->getID(), -triggerNormal);
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
