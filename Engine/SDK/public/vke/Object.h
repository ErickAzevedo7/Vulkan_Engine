#pragma once

#include <vke/Marks.h>
#include <vke/Math.h>

namespace vke {

    class Camera;
    class Light;
    class Mesh;
    class Transform;

    namespace detail {
        struct Bits;
    } // namespace detail

    // Which directions or which kind of position a function means.
    enum class Space {
        // The whole scene's.
        World,
        // The object's own, or its parent's.
        Local,
    };

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
        VKE_API void SetName(const char* name) const;

        // An inactive object, and everything under it, isn't drawn, doesn't collide and its scripts
        // don't run. This is the object's own switch: a parent can still turn it off.
        VKE_API bool IsActive() const;
        VKE_API void SetActive(bool active) const;

        // Where it is, how it moves and turns. Every object has one.
        VKE_API Transform GetTransform() const;

        // The object above this one, or nothing at the root of the scene.
        VKE_API Object GetParent() const;
        // Puts the object under `parent`, or at the root for an empty Object. With Space::World it
        // stays where it is in the scene; with Space::Local it keeps its position from the parent.
        VKE_API void SetParent(Object parent, Space keep = Space::World) const;
        VKE_API int GetChildCount() const;
        // The child in that place, from 0, or nothing.
        VKE_API Object GetChild(int index) const;

        // These parts of the object. Check IsValid on the result if it may not have one.
        VKE_API Light GetLight() const;
        VKE_API Camera GetCamera() const;
        VKE_API Mesh GetMesh() const;

        // Adds a script to the object and returns it, or the one it already has. Set its fields
        // right away: its OnStart runs at the start of the next frame.
        //
        //     vke::Object bullet = vke::CreateObject("Bullet");
        //     bullet.AddScript<Bullet>()->SetSpeed(20.0f);
        template<typename T>
        T* AddScript() const {
            return static_cast<T*>(AddScriptByName(T::kScriptName));
        }

        // The object's script of that type, or null. Use it right away: to keep it between frames
        // make the field a vke::ScriptRef, or keep the Object and ask again.
        //
        //     if (Health* health = collision.other.GetScript<Health>()) health->Hurt(10);
        template<typename T>
        T* GetScript() const {
            return static_cast<T*>(GetScriptByName(T::kScriptName));
        }

        bool operator==(const Object& other) const { return m_Handle == other.m_Handle; }
        bool operator!=(const Object& other) const { return m_Handle != other.m_Handle; }

    private:
        friend struct detail::Bits;

        VKE_API void* AddScriptByName(const char* name) const;
        VKE_API void* GetScriptByName(const char* name) const;

        unsigned long long m_Handle = ~0ull;
    };

    // A new, empty object at the root of the scene. Works while the game runs.
    VKE_API Object CreateObject(const char* name);

    // The first object with this name, looking through the whole scene, or nothing. Slow with
    // many objects: keep the result, or use a vke::ObjectRef field.
    VKE_API Object FindObject(const char* name);

    // Removes the object and everything under it at the end of the frame. It keeps working until
    // then, and its scripts get OnStop. Asking twice is fine.
    VKE_API void Destroy(Object object);

    // A field that points at an object of the scene. In the Inspector, drag the object onto it.
    class ObjectRef {
    public:
        // Whether the field points at something.
        bool IsSet() const { return m_ID != 0; }
        // The object, from OnStart on. Check IsValid: it can have been destroyed.
        Object Get() const;

    private:
        friend struct detail::Bits;

        // What is saved. Stays the first member: the engine reads it directly.
        unsigned long long m_ID = 0;
        // Filled in by the engine before OnStart.
        Object m_Object;
    };

    // A field that points at a script of another object, safe to keep. In the Inspector, drag the
    // object that has the script onto it.
    template<typename T>
    class ScriptRef {
    public:
        bool IsSet() const { return m_Object.IsSet(); }
        // The script, or null if it or its object is gone.
        T* Get() const { return m_Object.Get().template GetScript<T>(); }
        T* operator->() const { return Get(); }

    private:
        ObjectRef m_Object;
    };

    // Fields that point at an asset. In the Inspector, drag the asset onto them.
    class PrefabRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        friend struct detail::Bits;
        unsigned long long m_ID = 0;
    };

    class MeshRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        friend struct detail::Bits;
        unsigned long long m_ID = 0;
    };

    class MaterialRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        friend struct detail::Bits;
        unsigned long long m_ID = 0;
    };

    class SceneRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        friend struct detail::Bits;
        unsigned long long m_ID = 0;
    };

    // A new copy of a prefab at `position`, at the root of the scene. Its scripts start next
    // frame. Nothing if the field is empty.
    VKE_API Object Instantiate(PrefabRef prefab, Vec3 position = Vec3());

    namespace detail {
        // For the engine, which fills these in.
        struct Bits {
            static unsigned long long GetHandle(const Object& object) { return object.m_Handle; }
            static Object MakeObject(unsigned long long handle) {
                Object object;
                object.m_Handle = handle;
                return object;
            }
            static void SetObject(ObjectRef& reference, Object object) { reference.m_Object = object; }
            static Object GetObject(const ObjectRef& reference) { return reference.m_Object; }
            template<typename T>
            static unsigned long long GetID(const T& reference) {
                return reference.m_ID;
            }
        };
    } // namespace detail

    inline Object ObjectRef::Get() const {
        return m_Object;
    }

} // namespace vke
