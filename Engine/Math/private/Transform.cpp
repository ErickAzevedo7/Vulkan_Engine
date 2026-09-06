#include <math/Transform.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace VKE {
    Mat4 Transform::GetMatrix() const {
        return Compose(Position, Rotation, Scale);
    }

    Mat4 Transform::GetInverseMatrix() const {
        return glm::inverse(GetMatrix());
    }

    Vec3 Transform::GetForward() const {
        return glm::rotate(Rotation, Vec3(0.0f, 0.0f, -1.0f));
    }

    Vec3 Transform::GetRight() const {
        return glm::rotate(Rotation, Vec3(1.0f, 0.0f, 0.0f));
    }

    Vec3 Transform::GetUp() const {
        return glm::rotate(Rotation, Vec3(0.0f, 1.0f, 0.0f));
    }

    Mat4 Transform::Compose(const Vec3& position, const Quat& rotation, const Vec3& scale) {
        Mat4 translation = glm::translate(Mat4(1.0f), position);
        Mat4 rot = glm::mat4_cast(rotation);
        Mat4 sc = glm::scale(Mat4(1.0f), scale);
        return translation * rot * sc;
    }

    Transform Transform::Decompose(const Mat4& matrix) {
        Transform result;
        Vec3 skew;
        Vec4 perspective;
        glm::decompose(matrix, result.Scale, result.Rotation, result.Position, skew, perspective);
        return result;
    }
}
