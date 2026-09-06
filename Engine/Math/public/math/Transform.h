#pragma once

#include <math/MathTypes.h>

namespace VKE {
    struct Transform {
        Vec3 Position{0.0f, 0.0f, 0.0f};
        Quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Vec3 Scale{1.0f, 1.0f, 1.0f};

        Mat4 GetMatrix() const;
        Mat4 GetInverseMatrix() const;
        
        Vec3 GetForward() const;
        Vec3 GetRight() const;
        Vec3 GetUp() const;

        static Mat4 Compose(const Vec3& position, const Quat& rotation, const Vec3& scale);
        static Transform Decompose(const Mat4& matrix);
    };
}
