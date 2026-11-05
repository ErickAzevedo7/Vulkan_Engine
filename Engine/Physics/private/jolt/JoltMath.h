#pragma once

#include <math/MathTypes.h>

// Jolt.h goes before any other Jolt header.
#include <Jolt/Jolt.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

namespace VKE {

inline JPH::Vec3 ToJolt(const Vec3 &v) {
	return { v.x, v.y, v.z };
}

inline JPH::Float3 ToJoltFloat3(const Vec3 &v) {
	return { v.x, v.y, v.z };
}

inline JPH::Quat ToJolt(const Quat &q) {
	return { q.x, q.y, q.z, q.w };
}

inline Vec3 ToVKE(JPH::Vec3Arg v) {
	return { v.GetX(), v.GetY(), v.GetZ() };
}

inline Quat ToVKE(JPH::QuatArg q) {
	// glm takes w first.
	return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
}

} // namespace VKE
