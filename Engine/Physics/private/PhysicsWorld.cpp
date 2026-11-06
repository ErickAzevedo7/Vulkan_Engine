#include "physics/PhysicsWorld.h"

#include "Broadphase.h"
#include "Narrowphase.h"
#include "TriangleTree.h"
#include "jolt/JoltMath.h"
#include "jolt/JoltWorld.h"

#include <core/Log.h>
#include <core/SlotMap.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

#include <algorithm>
#include <unordered_map>

namespace VKE {

struct MeshShape {
	TriangleTree Tree;
	// The same triangles for Jolt. Null if Jolt couldn't build them.
	JPH::ShapeRefC Jolt;
};

struct Body {
	BodyDesc Desc;
	// Updated at the start of each step.
	AABB Bounds;
	bool HasShape = false;
	// Mesh bodies: the mesh's space to world, and back.
	Mat4 MeshToWorld{ 1.0f };
	Mat4 WorldToMesh{ 1.0f };
	// The body's copy in Jolt. Invalid while it has no shape Jolt can use.
	JPH::BodyID Jolt;
};

namespace {
// Bodies this close count as touching, so a body pushed out exactly to a surface keeps
// its contact instead of flickering between Begin and End.
constexpr f32 kContactMargin = 0.005f;
// Pushing a body out of one neighbor can push it into another; a few rounds settle that.
constexpr u32 kPushRounds = 4;
// Overlaps one body resolves in a round, one at a time.
constexpr u32 kPushesPerRound = 4;

struct TouchingPair {
	// Ordered by slot index, so the same pair always reports the same A and B.
	BodyID A;
	BodyID B;
	bool IsTrigger = false;
	Vec3 Normal{ 0.0f };
	u64 UserDataA = 0;
	u64 UserDataB = 0;

	ContactEvent ToEvent(ContactPhase phase) const {
		Vec3 normal = phase == ContactPhase::End ? Vec3(0.0f) : Normal;
		return { A, B, phase, IsTrigger, normal, UserDataA, UserDataB };
	}
};

u64 MakePairKey(BodyID a, BodyID b) {
	u32 low = std::min(a.Index, b.Index);
	u32 high = std::max(a.Index, b.Index);
	return (static_cast<u64>(low) << 32) | high;
}

bool IsStatic(const BodyDesc &desc) {
	return desc.Motion == BodyMotion::Static || desc.Shape == ShapeType::Mesh;
}

OrientedBox MakeBox(const BodyDesc &desc) {
	Mat3 rotation = glm::mat3_cast(desc.Pose.Rotation);
	OrientedBox box;
	for (int i = 0; i < 3; ++i) {
		box.Axes[i] = rotation[i];
	}
	box.HalfExtents = glm::abs(desc.HalfExtents * desc.Pose.Scale);
	box.Center = desc.Pose.Position + rotation * (desc.Center * desc.Pose.Scale);
	return box;
}

// Jolt can't build a box thinner than this.
constexpr f32 kMinJoltHalfExtent = 0.001f;

// Stored in each Jolt body, so its hits lead back to the engine's body.
u64 PackBodyID(BodyID id) {
	return (static_cast<u64>(id.Index) << 32) | id.Generation;
}

BodyID UnpackBodyID(u64 packed) {
	BodyID id;
	id.Index = static_cast<u32>(packed >> 32);
	id.Generation = static_cast<u32>(packed);
	return id;
}

class TriggerFilter final : public JPH::ObjectLayerFilter {
public:
	explicit TriggerFilter(bool withTriggers) : m_WithTriggers(withTriggers) {}

	bool ShouldCollide(JPH::ObjectLayer layer) const override { return m_WithTriggers || layer != JoltLayers::Trigger; }

private:
	bool m_WithTriggers;
};

JPH::ObjectLayer GetJoltLayer(const BodyDesc &desc) {
	if (desc.IsTrigger) {
		return JoltLayers::Trigger;
	}
	return IsStatic(desc) ? JoltLayers::Static : JoltLayers::Movable;
}

// A box body sits in Jolt at the box's center, so its shape needs no offset.
JPH::RVec3 GetJoltPosition(const BodyDesc &desc) {
	return ToJolt(desc.Shape == ShapeType::Box ? MakeBox(desc).Center : desc.Pose.Position);
}

// Whether the change from `before` to `after` only moved or turned the body, which Jolt can
// do in place; anything else rebuilds the Jolt body.
bool OnlyPlacementChanged(const BodyDesc &before, const BodyDesc &after) {
	return before.Shape == after.Shape && before.Center == after.Center && before.HalfExtents == after.HalfExtents &&
			before.Mesh == after.Mesh && before.Pose.Scale == after.Pose.Scale && before.Motion == after.Motion &&
			before.IsTrigger == after.IsTrigger;
}

AABB TransformBounds(const AABB &bounds, const Mat4 &matrix) {
	Vec3 first(matrix * Vec4(bounds.Min, 1.0f));
	AABB result(first, first);
	for (int corner = 1; corner < 8; ++corner) {
		Vec3 local((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
				(corner & 4) ? bounds.Max.z : bounds.Min.z);
		Vec3 world(matrix * Vec4(local, 1.0f));
		result.Min = glm::min(result.Min, world);
		result.Max = glm::max(result.Max, world);
	}
	return result;
}

Triangle TransformTriangle(const Triangle &triangle, const Mat4 &matrix) {
	return { Vec3(matrix * Vec4(triangle.A, 1.0f)), Vec3(matrix * Vec4(triangle.B, 1.0f)),
		Vec3(matrix * Vec4(triangle.C, 1.0f)) };
}

AABB Expand(const AABB &bounds, f32 amount) {
	return AABB(bounds.Min - Vec3(amount), bounds.Max + Vec3(amount));
}
} // namespace

struct PhysicsWorld::Storage {
	// Jolt's side of the world. Nothing is in it yet.
	JoltWorld Jolt;

	SlotMap<MeshShape> Shapes;
	SlotMap<Body> Bodies;

	Broadphase Pairs;
	std::vector<Broadphase::Entry> Entries;
	std::vector<Vec3> StartPositions;

	// Pairs that were touching at the end of the last step.
	std::unordered_map<u64, TouchingPair> Touching;
	std::unordered_map<u64, TouchingPair> NowTouching;
	// End events for destroyed bodies, reported by the next step.
	std::vector<ContactEvent> PendingEnds;

	std::vector<BodyID> Moved;
	std::vector<ContactEvent> Events;
	bool MovableMeshWarned = false;
	// Bodies entered Jolt since its broadphase was last rebuilt for fast queries.
	bool JoltBodiesAdded = false;

	Body &BodyAt(u32 denseIndex) { return *Bodies.Get(Bodies.HandleAt(denseIndex)); }

	void UpdateShape(Body &body) {
		const BodyDesc &desc = body.Desc;
		if (desc.Shape == ShapeType::Box) {
			body.HasShape = true;
			body.Bounds = GetBounds(MakeBox(desc));
			return;
		}

		const MeshShape *shape = Shapes.Get(desc.Mesh);
		body.HasShape = shape && !shape->Tree.IsEmpty();
		if (!body.HasShape) {
			return;
		}

		body.MeshToWorld = desc.Pose.GetMatrix();
		body.WorldToMesh = glm::inverse(body.MeshToWorld);
		body.Bounds = TransformBounds(shape->Tree.GetBounds(), body.MeshToWorld);
	}

	// Calls `visit(const Triangle&)` with each triangle of `mesh`, in world space, near `region`.
	template <typename Visitor> void VisitTriangles(const Body &mesh, const AABB &region, Visitor &&visit) {
		const MeshShape *shape = Shapes.Get(mesh.Desc.Mesh);
		shape->Tree.Query(TransformBounds(region, mesh.WorldToMesh),
				[&](const Triangle &triangle) { visit(TransformTriangle(triangle, mesh.MeshToWorld)); });
	}

	// Whether the pair is touching after the pushes, and the normal that moves `a` away from `b`.
	bool FindContact(const Body &a, const Body &b, Vec3 &outNormal) {
		if (a.Desc.Shape == ShapeType::Mesh || b.Desc.Shape == ShapeType::Mesh) {
			bool meshIsA = a.Desc.Shape == ShapeType::Mesh;
			const Body &box = meshIsA ? b : a;
			const Body &mesh = meshIsA ? a : b;

			OrientedBox shape = MakeBox(box.Desc);
			bool touching = false;
			f32 deepest = -kContactMargin;
			VisitTriangles(mesh, Expand(GetBounds(shape), kContactMargin), [&](const Triangle &triangle) {
				Penetration penetration;
				bool over = IsOverTriangle(shape.Center, triangle);
				if (!TestBoxTriangle(shape, triangle, kContactMargin, over, penetration)) {
					return;
				}
				if (!touching || penetration.Depth > deepest) {
					deepest = penetration.Depth;
					outNormal = meshIsA ? -penetration.Normal : penetration.Normal;
				}
				touching = true;
			});
			return touching;
		}

		Penetration penetration;
		if (!TestBoxBox(MakeBox(a.Desc), MakeBox(b.Desc), kContactMargin, penetration)) {
			return false;
		}
		outNormal = penetration.Normal;
		return true;
	}

	void EndContactsOf(BodyID id) {
		for (auto it = Touching.begin(); it != Touching.end();) {
			const TouchingPair &pair = it->second;
			if (pair.A == id || pair.B == id) {
				PendingEnds.push_back(pair.ToEvent(ContactPhase::End));
				it = Touching.erase(it);
			} else {
				++it;
			}
		}
	}

	JPH::BodyInterface &GetJoltBodies() { return Jolt.GetSystem().GetBodyInterface(); }

	// The body's shape for Jolt, or null if it has none Jolt can use.
	JPH::ShapeRefC MakeJoltShape(const BodyDesc &desc) const {
		if (desc.Shape == ShapeType::Box) {
			Vec3 halfExtents = glm::max(glm::abs(desc.HalfExtents * desc.Pose.Scale), Vec3(kMinJoltHalfExtent));
			// Jolt rounds a box's edges by its convex radius, which can't exceed its thinnest side.
			f32 thinnest = std::min({ halfExtents.x, halfExtents.y, halfExtents.z });
			return new JPH::BoxShape(ToJolt(halfExtents), std::min(JPH::cDefaultConvexRadius, 0.5f * thinnest));
		}

		const MeshShape *shape = Shapes.Get(desc.Mesh);
		JPH::Vec3 scale = ToJolt(desc.Pose.Scale);
		if (!shape || !shape->Jolt || JPH::ScaleHelpers::IsZeroScale(scale)) {
			return nullptr;
		}
		if (JPH::ScaleHelpers::IsNotScaled(scale)) {
			return shape->Jolt;
		}
		return new JPH::ScaledShape(shape->Jolt, scale);
	}

	void AddToJolt(BodyID id, Body &body) {
		JPH::ShapeRefC shape = MakeJoltShape(body.Desc);
		if (!shape) {
			return;
		}

		JPH::ObjectLayer layer = GetJoltLayer(body.Desc);
		JPH::EMotionType motion = layer == JoltLayers::Static ? JPH::EMotionType::Static : JPH::EMotionType::Kinematic;
		JPH::BodyCreationSettings settings(
				shape, GetJoltPosition(body.Desc), ToJolt(glm::normalize(body.Desc.Pose.Rotation)), motion, layer);
		settings.mIsSensor = body.Desc.IsTrigger;
		settings.mUserData = PackBodyID(id);
		body.Jolt = GetJoltBodies().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
		if (body.Jolt.IsInvalid()) {
			VKE_CORE_WARN("PhysicsWorld: Jolt is out of bodies");
			return;
		}
		JoltBodiesAdded = true;
	}

	void RemoveFromJolt(Body &body) {
		if (body.Jolt.IsInvalid()) {
			return;
		}
		GetJoltBodies().RemoveBody(body.Jolt);
		GetJoltBodies().DestroyBody(body.Jolt);
		body.Jolt = {};
	}

	// Brings the body's Jolt copy up to its desc, which was `before`.
	void UpdateInJolt(BodyID id, Body &body, const BodyDesc &before) {
		if (!body.Jolt.IsInvalid() && OnlyPlacementChanged(before, body.Desc)) {
			MoveInJolt(body);
			return;
		}
		RemoveFromJolt(body);
		AddToJolt(id, body);
	}

	void MoveInJolt(const Body &body) {
		if (!body.Jolt.IsInvalid()) {
			GetJoltBodies().SetPositionAndRotation(body.Jolt, GetJoltPosition(body.Desc),
					ToJolt(glm::normalize(body.Desc.Pose.Rotation)), JPH::EActivation::DontActivate);
		}
	}

	// Pushes a movable body out of the solid bodies it overlaps, the deepest overlap first, so
	// a floor and a wall both hold it once the rest is resolved. A movable neighbor takes half
	// of each push. Returns true if anything moved.
	bool PushOut(Body &body) {
		if (body.Jolt.IsInvalid()) {
			return false;
		}

		JPH::BodyInterface &bodies = GetJoltBodies();
		bool pushed = false;
		for (u32 i = 0; i < kPushesPerRound; ++i) {
			JPH::RefConst<JPH::Shape> shape = bodies.GetShape(body.Jolt);
			JPH::RMat44 transform = bodies.GetCenterOfMassTransform(body.Jolt);
			JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
			Jolt.GetSystem().GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), transform,
					JPH::CollideShapeSettings(), transform.GetTranslation(), hits, {}, TriggerFilter(false),
					JPH::IgnoreSingleBodyFilter(body.Jolt));

			const JPH::CollideShapeResult *deepest = nullptr;
			for (const JPH::CollideShapeResult &hit : hits.mHits) {
				if (hit.mPenetrationDepth > 0.0f && hit.mPenetrationAxis.LengthSq() > 0.0f &&
						(!deepest || hit.mPenetrationDepth > deepest->mPenetrationDepth)) {
					deepest = &hit;
				}
			}
			if (!deepest) {
				break;
			}

			// The axis moves the other body out, so this one goes the opposite way.
			Vec3 push = -ToVKE(deepest->mPenetrationAxis.Normalized()) * deepest->mPenetrationDepth;
			Body *other = Bodies.Get(UnpackBodyID(bodies.GetUserData(deepest->mBodyID2)));
			if (other && !IsStatic(other->Desc)) {
				push *= 0.5f;
				other->Desc.Pose.Position -= push;
				MoveInJolt(*other);
			}
			body.Desc.Pose.Position += push;
			MoveInJolt(body);
			pushed = true;
		}
		return pushed;
	}
};

PhysicsWorld::PhysicsWorld() : m_Storage(MakeIunique<Storage>()) {}

PhysicsWorld::~PhysicsWorld() = default;

MeshShapeID PhysicsWorld::CreateMeshShape(const MeshShapeDesc &desc) {
	MeshShape shape;
	shape.Tree.Build(desc.Positions, desc.Indices);
	if (shape.Tree.IsEmpty()) {
		VKE_CORE_WARN("PhysicsWorld: mesh shape has no triangles");
		return m_Storage->Shapes.Emplace(std::move(shape));
	}

	JPH::VertexList vertices;
	vertices.reserve(desc.Positions.size());
	for (const Vec3 &position : desc.Positions) {
		vertices.push_back(ToJoltFloat3(position));
	}
	JPH::IndexedTriangleList triangles;
	triangles.reserve(desc.Indices.size() / 3);
	for (size_t i = 0; i + 2 < desc.Indices.size(); i += 3) {
		triangles.emplace_back(desc.Indices[i], desc.Indices[i + 1], desc.Indices[i + 2]);
	}

	JPH::ShapeSettings::ShapeResult result = JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
	if (result.HasError()) {
		VKE_CORE_WARN("PhysicsWorld: Jolt couldn't build a mesh shape: {}", result.GetError().c_str());
	} else {
		shape.Jolt = result.Get();
	}
	return m_Storage->Shapes.Emplace(std::move(shape));
}

void PhysicsWorld::DestroyMeshShape(MeshShapeID id) {
	Storage &s = *m_Storage;
	// A Jolt body keeps its own reference to the shape, so it has to leave too.
	const u32 bodyCount = static_cast<u32>(s.Bodies.Size());
	for (u32 i = 0; i < bodyCount; ++i) {
		Body &body = s.BodyAt(i);
		if (body.Desc.Shape == ShapeType::Mesh && body.Desc.Mesh == id) {
			s.RemoveFromJolt(body);
		}
	}
	s.Shapes.Remove(id);
}

BodyID PhysicsWorld::CreateBody(const BodyDesc &desc) {
	Body body;
	body.Desc = desc;
	BodyID id = m_Storage->Bodies.Emplace(body);
	m_Storage->AddToJolt(id, *m_Storage->Bodies.Get(id));
	return id;
}

void PhysicsWorld::SetBody(BodyID id, const BodyDesc &desc) {
	if (Body *body = m_Storage->Bodies.Get(id)) {
		BodyDesc before = body->Desc;
		body->Desc = desc;
		m_Storage->UpdateInJolt(id, *body, before);
	}
}

void PhysicsWorld::SetPose(BodyID id, const Transform &pose) {
	if (Body *body = m_Storage->Bodies.Get(id)) {
		BodyDesc before = body->Desc;
		body->Desc.Pose = pose;
		m_Storage->UpdateInJolt(id, *body, before);
	}
}

void PhysicsWorld::DestroyBody(BodyID id) {
	Body *body = m_Storage->Bodies.Get(id);
	if (!body) {
		return;
	}
	m_Storage->RemoveFromJolt(*body);
	m_Storage->Bodies.Remove(id);
	m_Storage->EndContactsOf(id);
}

const BodyDesc *PhysicsWorld::GetBody(BodyID id) const {
	const Body *body = m_Storage->Bodies.Get(id);
	return body ? &body->Desc : nullptr;
}

bool PhysicsWorld::Raycast(
		const Vec3 &origin, const Vec3 &direction, f32 maxDistance, bool withTriggers, RayHit &outHit) const {
	if (glm::dot(direction, direction) == 0.0f || maxDistance <= 0.0f) {
		return false;
	}
	Vec3 unit = glm::normalize(direction);

	const JPH::PhysicsSystem &system = m_Storage->Jolt.GetSystem();
	// Jolt's ray runs from the origin to origin + direction.
	JPH::RRayCast ray(ToJolt(origin), ToJolt(unit * maxDistance));
	JPH::RayCastSettings settings;
	// A ray that starts inside a box only meets its far side, which faces away and is skipped.
	settings.mTreatConvexAsSolid = false;
	// Both faces of a mesh's triangles count.
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> closest;
	system.GetNarrowPhaseQuery().CastRay(ray, settings, closest, {}, TriggerFilter(withTriggers));
	if (!closest.HadHit()) {
		return false;
	}

	const JPH::RayCastResult &hit = closest.mHit;
	JPH::BodyLockRead lock(system.GetBodyLockInterface(), hit.mBodyID);
	if (!lock.Succeeded()) {
		return false;
	}
	BodyID id = UnpackBodyID(lock.GetBody().GetUserData());
	const Body *body = m_Storage->Bodies.Get(id);
	if (!body) {
		return false;
	}

	f32 distance = hit.mFraction * maxDistance;
	Vec3 point = origin + unit * distance;
	Vec3 normal = ToVKE(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJolt(point)));
	// A mesh's back face gives the normal on its other side.
	if (glm::dot(normal, unit) > 0.0f) {
		normal = -normal;
	}
	outHit = { id, point, normal, distance, body->Desc.UserData };
	return true;
}

void PhysicsWorld::Step() {
	Storage &s = *m_Storage;
	s.Moved.clear();
	s.Events.swap(s.PendingEnds);
	s.PendingEnds.clear();

	if (s.JoltBodiesAdded) {
		s.Jolt.GetSystem().OptimizeBroadPhase();
		s.JoltBodiesAdded = false;
	}

	const u32 bodyCount = static_cast<u32>(s.Bodies.Size());
	s.Entries.clear();
	s.StartPositions.resize(bodyCount);
	for (u32 i = 0; i < bodyCount; ++i) {
		Body &body = s.BodyAt(i);
		if (body.Desc.Shape == ShapeType::Mesh && body.Desc.Motion == BodyMotion::Movable && !s.MovableMeshWarned) {
			VKE_CORE_WARN("PhysicsWorld: mesh bodies can't be movable and are treated as static");
			s.MovableMeshWarned = true;
		}

		s.UpdateShape(body);
		s.StartPositions[i] = body.Desc.Pose.Position;
		// Grown by the margin, so pairs that only come close are still checked for contact.
		if (body.HasShape) {
			s.Entries.push_back({ Expand(body.Bounds, kContactMargin), i, IsStatic(body.Desc) });
		}
	}

	// A mesh is never pushed, so two meshes never need comparing.
	std::vector<std::pair<u32, u32>> pairs;
	for (const auto &[a, b] : s.Pairs.FindPairs(s.Entries)) {
		if (s.BodyAt(a).Desc.Shape == ShapeType::Mesh && s.BodyAt(b).Desc.Shape == ShapeType::Mesh) {
			continue;
		}
		pairs.emplace_back(a, b);
	}

	for (u32 round = 0; round < kPushRounds; ++round) {
		bool pushed = false;
		for (u32 i = 0; i < bodyCount; ++i) {
			Body &body = s.BodyAt(i);
			if (!IsStatic(body.Desc) && !body.Desc.IsTrigger) {
				pushed |= s.PushOut(body);
			}
		}
		if (!pushed) {
			break;
		}
	}

	for (u32 i = 0; i < bodyCount; ++i) {
		if (s.BodyAt(i).Desc.Pose.Position != s.StartPositions[i]) {
			s.Moved.push_back(s.Bodies.HandleAt(i));
		}
	}

	s.NowTouching.clear();
	for (const auto &[a, b] : pairs) {
		BodyID idA = s.Bodies.HandleAt(a);
		BodyID idB = s.Bodies.HandleAt(b);
		u32 first = a;
		u32 second = b;
		if (idB.Index < idA.Index) {
			std::swap(idA, idB);
			std::swap(first, second);
		}

		const Body &bodyA = s.BodyAt(first);
		const Body &bodyB = s.BodyAt(second);
		TouchingPair pair{ idA, idB, bodyA.Desc.IsTrigger || bodyB.Desc.IsTrigger, Vec3(0.0f), bodyA.Desc.UserData,
			bodyB.Desc.UserData };
		Vec3 normal;
		if (!s.FindContact(bodyA, bodyB, normal)) {
			continue;
		}
		if (!pair.IsTrigger) {
			pair.Normal = normal;
		}
		s.NowTouching.emplace(MakePairKey(idA, idB), pair);
	}

	for (const auto &[key, pair] : s.Touching) {
		auto now = s.NowTouching.find(key);
		// A pair that turned into a trigger pair, or back, ends and begins again.
		if (now == s.NowTouching.end() || now->second.IsTrigger != pair.IsTrigger) {
			s.Events.push_back(pair.ToEvent(ContactPhase::End));
		}
	}
	for (const auto &[key, pair] : s.NowTouching) {
		auto before = s.Touching.find(key);
		bool continues = before != s.Touching.end() && before->second.IsTrigger == pair.IsTrigger;
		s.Events.push_back(pair.ToEvent(continues ? ContactPhase::Stay : ContactPhase::Begin));
	}
	s.Touching.swap(s.NowTouching);
}

const std::vector<BodyID> &PhysicsWorld::GetMovedBodies() const {
	return m_Storage->Moved;
}

const std::vector<ContactEvent> &PhysicsWorld::GetContactEvents() const {
	return m_Storage->Events;
}

} // namespace VKE
