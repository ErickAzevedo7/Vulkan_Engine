#pragma once

#include <core/Types.h>

// Jolt.h goes before any other Jolt header.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace VKE {

// What each body is to Jolt's filters.
namespace JoltLayers {
constexpr JPH::ObjectLayer Static = 0;
constexpr JPH::ObjectLayer Movable = 1;
// Triggers can be moved by their objects, so they sit with the moving bodies.
constexpr JPH::ObjectLayer Trigger = 2;
constexpr JPH::ObjectLayer Count = 3;

constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint BroadPhaseCount = 2;
} // namespace JoltLayers

class JoltBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
	JPH::uint GetNumBroadPhaseLayers() const override;
	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
};

class JoltObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
	bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override;
};

// Everything meets everything, except two static bodies, which never move.
class JoltObjectPairFilter final : public JPH::ObjectLayerPairFilter {
public:
	bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
};

// A Jolt physics system set up with the engine's layers. Jolt's global state (allocator,
// factory, registered types) lives while any JoltWorld exists.
class JoltWorld {
public:
	JoltWorld();
	~JoltWorld();

	JoltWorld(const JoltWorld &) = delete;
	JoltWorld &operator=(const JoltWorld &) = delete;

	JPH::PhysicsSystem &GetSystem() { return *m_System; }
	const JPH::PhysicsSystem &GetSystem() const { return *m_System; }

private:
	// The system keeps references to these, so they are declared, and live, before it.
	JoltBroadPhaseLayers m_BroadPhaseLayers;
	JoltObjectVsBroadPhaseFilter m_ObjectVsBroadPhase;
	JoltObjectPairFilter m_ObjectPairs;
	Iunique<JPH::PhysicsSystem> m_System;
};

} // namespace VKE
