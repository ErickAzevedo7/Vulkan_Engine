#include "JoltWorld.h"

#include <core/Log.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace VKE {

namespace {
constexpr JPH::uint kMaxBodies = 65536;
// 0 lets Jolt pick.
constexpr JPH::uint kBodyMutexes = 0;
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;

std::mutex s_JoltMutex;
u32 s_JoltUsers = 0;

void TraceToLog(const char *format, ...) {
	char message[1024];
	va_list args;
	va_start(args, format);
	std::vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	VKE_CORE_TRACE("Jolt: {}", message);
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertToLog(const char *expression, const char *message, const char *file, JPH::uint line) {
	VKE_CORE_ERROR("Jolt assert failed: {} {} ({}:{})", expression, message ? message : "", file, line);
	// Break, like VKE_CORE_ASSERT.
	return true;
}
#endif

void AcquireJolt() {
	std::lock_guard lock(s_JoltMutex);
	if (s_JoltUsers++ > 0) {
		return;
	}

	JPH::RegisterDefaultAllocator();
	JPH::Trace = TraceToLog;
#ifdef JPH_ENABLE_ASSERTS
	JPH::AssertFailed = AssertToLog;
#endif
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();
}

void ReleaseJolt() {
	std::lock_guard lock(s_JoltMutex);
	if (--s_JoltUsers > 0) {
		return;
	}

	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}
} // namespace

JPH::uint JoltBroadPhaseLayers::GetNumBroadPhaseLayers() const {
	return JoltLayers::BroadPhaseCount;
}

JPH::BroadPhaseLayer JoltBroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
	return layer == JoltLayers::Static ? JoltLayers::NonMoving : JoltLayers::Moving;
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char *JoltBroadPhaseLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const {
	return layer == JoltLayers::NonMoving ? "NonMoving" : "Moving";
}
#endif

bool JoltObjectVsBroadPhaseFilter::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const {
	return layer != JoltLayers::Static || broadPhaseLayer != JoltLayers::NonMoving;
}

bool JoltObjectPairFilter::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const {
	return a != JoltLayers::Static || b != JoltLayers::Static;
}

JoltWorld::JoltWorld() {
	AcquireJolt();
	m_System = MakeIunique<JPH::PhysicsSystem>();
	m_System->Init(kMaxBodies, kBodyMutexes, kMaxBodyPairs, kMaxContactConstraints, m_BroadPhaseLayers,
			m_ObjectVsBroadPhase, m_ObjectPairs);
}

JoltWorld::~JoltWorld() {
	m_System.reset();
	ReleaseJolt();
}

} // namespace VKE
