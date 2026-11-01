#pragma once

#include <vke/Marks.h>

#include <cmath>

namespace vke {

    // A position, a direction or a color. Distances are in meters.
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vec3() = default;
        constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

        constexpr Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
        constexpr Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
        constexpr Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
        constexpr Vec3 operator-() const { return {-x, -y, -z}; }
        Vec3& operator+=(const Vec3& other) { return *this = *this + other; }
        Vec3& operator-=(const Vec3& other) { return *this = *this - other; }
        Vec3& operator*=(float scale) { return *this = *this * scale; }
        constexpr bool operator==(const Vec3& other) const { return x == other.x && y == other.y && z == other.z; }
        constexpr bool operator!=(const Vec3& other) const { return !(*this == other); }

        float Length() const { return std::sqrt(x * x + y * y + z * z); }
        // The same direction with length 1, or zero for a zero vector.
        Vec3 Normalized() const {
            float length = Length();
            return length > 0.0f ? *this * (1.0f / length) : Vec3();
        }

        static constexpr float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        static constexpr Vec3 Cross(const Vec3& a, const Vec3& b) {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        // From `a` at 0 to `b` at 1.
        static constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, float amount) { return a + (b - a) * amount; }
    };

    // A rotation. Starts as no rotation at all. Most code only needs Transform::Rotate.
    struct Quat {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        // Turns around X, Y and Z, in degrees.
        VKE_API static Quat FromDegrees(float x, float y, float z);
        // The same turns back, in degrees.
        VKE_API Vec3 ToDegrees() const;
        // `vector` turned by this rotation.
        VKE_API Vec3 Rotate(Vec3 vector) const;
    };

} // namespace vke
