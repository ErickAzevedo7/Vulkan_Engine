#pragma once

#include <vke/Marks.h>

namespace vke {

    namespace detail {
        struct ObjectBits;
    } // namespace detail

    // Something in the scene. Safe to keep: once the object is destroyed, IsValid gives false and
    // the rest does nothing.
    class Object {
    public:
        // Points at nothing.
        Object() = default;

        // Whether the object is still in the scene.
        VKE_API bool IsValid() const;
        // The object's name, or an empty text. Keep a copy to use it after the object changes.
        VKE_API const char* GetName() const;

        // Adds a script to the object and returns it, or the one it already has. Set its fields
        // right away: its OnStart runs at the start of the next frame.
        //
        //     vke::Object bullet = vke::CreateObject("Bullet");
        //     bullet.AddScript<Bullet>()->SetSpeed(20.0f);
        template<typename T>
        T* AddScript() const {
            return static_cast<T*>(AddScriptByName(T::kScriptName));
        }

        bool operator==(const Object& other) const { return m_Handle == other.m_Handle; }
        bool operator!=(const Object& other) const { return m_Handle != other.m_Handle; }

    private:
        friend struct detail::ObjectBits;

        VKE_API void* AddScriptByName(const char* name) const;

        unsigned long long m_Handle = ~0ull;
    };

    // A new, empty object at the root of the scene. Works while the game runs.
    VKE_API Object CreateObject(const char* name);

    // Removes the object and everything under it at the end of the frame. It keeps working until
    // then, and its scripts get OnStop. Asking twice is fine.
    VKE_API void Destroy(Object object);

    // A field that points at an object of the scene. In the Inspector, drag the object onto it.
    class ObjectRef {
    public:
        // Whether the field points at something. The object can still have been destroyed.
        bool IsSet() const { return m_ID != 0; }

    private:
        unsigned long long m_ID = 0;
    };

    // A field that points at a prefab. In the Inspector, drag the prefab onto it.
    class PrefabRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        unsigned long long m_ID = 0;
    };

    namespace detail {
        // For the engine, which fills objects in.
        struct ObjectBits {
            static unsigned long long Get(const Object& object) { return object.m_Handle; }
            static Object Make(unsigned long long handle) {
                Object object;
                object.m_Handle = handle;
                return object;
            }
        };
    } // namespace detail

} // namespace vke
