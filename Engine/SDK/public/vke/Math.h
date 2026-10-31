#pragma once

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
        constexpr bool operator==(const Vec3& other) const { return x == other.x && y == other.y && z == other.z; }
        constexpr bool operator!=(const Vec3& other) const { return !(*this == other); }
    };

    // A rotation. Starts as no rotation at all.
    struct Quat {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

} // namespace vke
