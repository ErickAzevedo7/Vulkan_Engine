#pragma once

#include <vke/Marks.h>
#include <vke/Math.h>
#include <vke/Object.h>

namespace vke {

    // The light on an object, from Object::GetLight. Safe to keep. Without a light, IsValid gives
    // false and the rest does nothing.
    class Light {
    public:
        Light() = default;
        explicit Light(Object object) : m_Object(object) {}

        Object GetObject() const { return m_Object; }
        VKE_API bool IsValid() const;

        VKE_API Vec3 GetColor() const;
        VKE_API void SetColor(Vec3 color) const;
        VKE_API float GetIntensity() const;
        VKE_API void SetIntensity(float intensity) const;
        // How far a point or spot light reaches, in meters.
        VKE_API float GetRange() const;
        VKE_API void SetRange(float range) const;

    private:
        Object m_Object;
    };

    // The camera on an object, from Object::GetCamera. Safe to keep.
    class Camera {
    public:
        Camera() = default;
        explicit Camera(Object object) : m_Object(object) {}

        Object GetObject() const { return m_Object; }
        VKE_API bool IsValid() const;

        // From the bottom to the top of the view, in degrees.
        VKE_API float GetFieldOfView() const;
        VKE_API void SetFieldOfView(float degrees) const;
        // Makes the game show what this camera sees.
        VKE_API void MakeMain() const;

    private:
        Object m_Object;
    };

    // The mesh on an object, from Object::GetMesh. Safe to keep.
    class Mesh {
    public:
        Mesh() = default;
        explicit Mesh(Object object) : m_Object(object) {}

        Object GetObject() const { return m_Object; }
        VKE_API bool IsValid() const;

        // Draws another mesh in its place.
        VKE_API void SetMesh(MeshRef mesh) const;
        // Draws one part of the mesh, the first by default, with another material.
        VKE_API void SetMaterial(MaterialRef material, int slot = 0) const;

    private:
        Object m_Object;
    };

} // namespace vke
