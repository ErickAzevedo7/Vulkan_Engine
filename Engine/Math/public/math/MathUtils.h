#pragma once

#include <cmath>
#include <algorithm>
#include <math/MathTypes.h>

namespace VKE {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = 0.5f * PI;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;

    template <typename T>
    constexpr T Radians(T degrees) { return degrees * static_cast<T>(DEG_TO_RAD); }

    template <typename T>
    constexpr T Degrees(T radians) { return radians * static_cast<T>(RAD_TO_DEG); }

    template <typename T>
    constexpr T Min(T a, T b) { return (a < b) ? a : b; }

    template <typename T>
    constexpr T Max(T a, T b) { return (a > b) ? a : b; }

    template <typename T>
    constexpr T Clamp(T val, T min, T max) {
        return (val < min) ? min : ((val > max) ? max : val);
    }

    template <typename T, typename U>
    constexpr T Lerp(T a, T b, U t) {
        return a + static_cast<T>((b - a) * t);
    }
}
