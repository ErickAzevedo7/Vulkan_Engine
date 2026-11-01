#include "ApiCommon.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include <vke/detail/ApiBindings.h>

namespace vke::api {

    namespace {
        using VKE::Scripting::ToEngine;
        using VKE::Scripting::ToSDK;

        VKE::Scene::TransformComponent* Find(Transform self) {
            VKE::Scene::GameObject* object = VKE::Scripting::FindObject(self.GetObject());
            return object ? &object->GetTransform() : nullptr;
        }

        VKE::Quat FromDegrees(float x, float y, float z) {
            return VKE::Quat(glm::radians(VKE::Vec3(x, y, z)));
        }
    } // namespace

    Quat Quat_FromDegrees(float x, float y, float z) {
        return ToSDK(FromDegrees(x, y, z));
    }

    Vec3 Quat_ToDegrees(Quat self) {
        return ToSDK(VKE::Vec3(glm::degrees(glm::eulerAngles(ToEngine(self)))));
    }

    Vec3 Quat_Rotate(Quat self, Vec3 vector) {
        return ToSDK(VKE::Vec3(ToEngine(self) * ToEngine(vector)));
    }

    Vec3 Transform_GetPosition(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetWorld().Position) : Vec3();
    }

    void Transform_SetPosition(Transform self, Vec3 position) {
        if (auto* transform = Find(self)) transform->SetWorldPosition(ToEngine(position));
    }

    Vec3 Transform_GetLocalPosition(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetLocal().Position) : Vec3();
    }

    void Transform_SetLocalPosition(Transform self, Vec3 position) {
        if (auto* transform = Find(self)) transform->SetLocalPosition(ToEngine(position));
    }

    Quat Transform_GetRotation(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetWorld().Rotation) : Quat();
    }

    void Transform_SetRotation(Transform self, Quat rotation) {
        if (auto* transform = Find(self)) transform->SetWorldRotation(glm::normalize(ToEngine(rotation)));
    }

    Quat Transform_GetLocalRotation(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetLocal().Rotation) : Quat();
    }

    void Transform_SetLocalRotation(Transform self, Quat rotation) {
        if (auto* transform = Find(self)) transform->SetLocalRotation(glm::normalize(ToEngine(rotation)));
    }

    Vec3 Transform_GetScale(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetLocal().Scale) : Vec3(1.0f, 1.0f, 1.0f);
    }

    void Transform_SetScale(Transform self, Vec3 scale) {
        if (auto* transform = Find(self)) transform->SetLocalScale(ToEngine(scale));
    }

    void Transform_Translate(Transform self, Vec3 offset, Space space) {
        auto* transform = Find(self);
        if (!transform) return;
        const VKE::Transform& world = transform->GetWorld();
        VKE::Vec3 move = space == Space::Local ? world.Rotation * ToEngine(offset) : ToEngine(offset);
        transform->SetWorldPosition(world.Position + move);
    }

    void Transform_Rotate(Transform self, float x, float y, float z, Space space) {
        auto* transform = Find(self);
        if (!transform) return;
        VKE::Quat turn = FromDegrees(x, y, z);
        VKE::Quat current = transform->GetWorld().Rotation;
        transform->SetWorldRotation(glm::normalize(space == Space::Local ? current * turn : turn * current));
    }

    void Transform_LookAt(Transform self, Vec3 target) {
        auto* transform = Find(self);
        if (!transform) return;
        VKE::Vec3 toTarget = ToEngine(target) - transform->GetWorld().Position;
        if (glm::dot(toTarget, toTarget) == 0.0f) return;
        toTarget = glm::normalize(toTarget);
        // Straight up or down, any top will do.
        VKE::Vec3 up = std::abs(toTarget.y) > 0.999f ? VKE::Vec3(0.0f, 0.0f, 1.0f) : VKE::Vec3(0.0f, 1.0f, 0.0f);
        transform->SetWorldRotation(glm::quatLookAt(toTarget, up));
    }

    Vec3 Transform_GetForward(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetWorld().GetForward()) : Vec3();
    }

    Vec3 Transform_GetRight(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetWorld().GetRight()) : Vec3();
    }

    Vec3 Transform_GetUp(Transform self) {
        auto* transform = Find(self);
        return transform ? ToSDK(transform->GetWorld().GetUp()) : Vec3();
    }

} // namespace vke::api
