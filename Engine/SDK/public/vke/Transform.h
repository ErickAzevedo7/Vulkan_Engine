#pragma once

#include <vke/Marks.h>
#include <vke/Math.h>
#include <vke/Object.h>

namespace vke {

    // Where an object is, how it is turned and how big it is. Comes from Object::GetTransform, or
    // GetTransform in a script. Positions and rotations are the scene's; the ones with Local in
    // the name are relative to the parent.
    class Transform {
    public:
        Transform() = default;
        explicit Transform(Object object) : m_Object(object) {}

        Object GetObject() const { return m_Object; }

        VKE_API Vec3 GetPosition() const;
        VKE_API void SetPosition(Vec3 position) const;
        VKE_API Vec3 GetLocalPosition() const;
        VKE_API void SetLocalPosition(Vec3 position) const;

        VKE_API Quat GetRotation() const;
        VKE_API void SetRotation(Quat rotation) const;
        VKE_API Quat GetLocalRotation() const;
        VKE_API void SetLocalRotation(Quat rotation) const;

        // How big the object is against its parent. 1 is its own size.
        VKE_API Vec3 GetScale() const;
        VKE_API void SetScale(Vec3 scale) const;

        // Moves by `offset`, along the scene's axes or, with Space::Local, the object's own:
        //
        //     GetTransform().Translate({0, 0, -speed * deltaTime}, vke::Space::Local);
        VKE_API void Translate(Vec3 offset, Space space = Space::World) const;
        // Turns around X, Y and Z, in degrees, around the object's own axes or, with Space::World,
        // the scene's.
        VKE_API void Rotate(float x, float y, float z, Space space = Space::Local) const;
        // Turns the object so its front faces `target`, keeping its top up.
        VKE_API void LookAt(Vec3 target) const;

        // Where the object's front, right and top face in the scene, as vectors of length 1.
        VKE_API Vec3 GetForward() const;
        VKE_API Vec3 GetRight() const;
        VKE_API Vec3 GetUp() const;

    private:
        Object m_Object;
    };

} // namespace vke
