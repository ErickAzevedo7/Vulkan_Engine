#pragma once

#include <scene/Properties.h>

#include <core/Assert.h>
#include <core/Types.h>
#include <reflection/Marks.h>

#include <string_view>
#include <type_traits>

namespace VKE::Scene {

    class Component;
    class ComponentRegistry;

    // A small number the registry gives each component type, used to find components quickly.
    // Only valid within one run: files name component types by their registered name.
    using ComponentTypeID = u32;
    constexpr ComponentTypeID kInvalidComponentType = ~ComponentTypeID(0);

    // Declares a component type. Goes first in the class body:
    //
    //     class LightComponent final : public Component {
    //         VKE_COMPONENT(LightComponent)
    //     public:
    //         ...
    //     private:
    //         VKE_PROPERTY() LightSettings m_Settings;
    //     };
    //
    // The reflection generator registers every class it finds this in, with the members marked
    // VKE_PROPERTY as its properties; the registry is a friend, so the generated tables can reach
    // private members. The class name becomes the type's name in saved scenes, so renaming a
    // component class breaks the scenes that use it. After the name, the generator takes the
    // option NotAddable, for types an object only gets on its own, like the transform.
#define VKE_COMPONENT(Type, ...)                                                                                       \
public:                                                                                                                \
    using ComponentSelf = Type;                                                                                        \
    static constexpr const char* kComponentTypeName = #Type;                                                           \
    ::VKE::Scene::ComponentTypeID GetTypeID() const override { return s_ComponentTypeID; }                            \
                                                                                                                       \
private:                                                                                                               \
    friend class ::VKE::Scene::ComponentRegistry;                                                                      \
    static inline ::VKE::Scene::ComponentTypeID s_ComponentTypeID = ::VKE::Scene::kInvalidComponentType;

    struct ComponentTypeInfo {
        ComponentTypeID ID = kInvalidComponentType;
        const char* Name = nullptr;
        // Null for types that can't be added by name.
        Component* (*Create)(const ComponentTypeInfo& type) = nullptr;
        // For Create, when the type isn't a class of its own, like a game's script.
        const void* Context = nullptr;
        const StructInfo* Properties = nullptr;
        // The object the properties describe, behind a component of this type.
        void* (*ToObject)(Component*) = nullptr;
        // A default-constructed component, never part of a scene: what properties are worth
        // saving is decided by comparing against it.
        Iunique<Component> Defaults;
        // Stands for a type that isn't there: its components are MissingComponents.
        bool Missing = false;
    };

    // Every component type the engine knows, by ID and by name.
    class ComponentRegistry {
    public:
        // Registers the engine's own component types. Call once at startup, before any scene is
        // created and before types from other modules are registered. Its body is written by the
        // reflection generator, into the build folder (VKE_Scene.reflection.cpp).
        static void RegisterEngineComponents();

        // Gives T an ID and its properties. With `addable`, the type can be added by name, as a
        // scene file does. Registering a type twice, or two types with the same name, is an error.
        template<typename T>
        static void Register(const StructInfo& properties, bool addable = true) {
            static_assert(std::is_same_v<typename T::ComponentSelf, T>, "T is missing VKE_COMPONENT(T)");
            static_assert(std::is_default_constructible_v<T>, "components need a default constructor");
            VKE_CORE_ASSERT(T::s_ComponentTypeID == kInvalidComponentType, "Component type registered twice");

            ComponentTypeInfo info;
            info.Name = T::kComponentTypeName;
            if (addable) info.Create = [](const ComponentTypeInfo&) { return static_cast<Component*>(new T()); };
            info.Properties = &properties;
            info.ToObject = [](Component* component) -> void* { return static_cast<T*>(component); };
            info.Defaults.reset(new T());
            T::s_ComponentTypeID = Add(std::move(info));
        }

        // A type defined outside the engine, like a game's script, for the code that loads it.
        // `info.Name` is copied. The ID is the one the name already has, if it was a missing or
        // released type, so components keep their type across reloads. Invalid if an engine
        // type has the name.
        static ComponentTypeID RegisterExternal(ComponentTypeInfo info);
        // The type stops being there: it turns into a missing type, with the same ID.
        // Call with no components of it left, as the code they run may be going away.
        static void ReleaseExternal(ComponentTypeID id);
        // The missing type called `name`, added if no type has that name yet.
        static const ComponentTypeInfo& FindOrAddMissing(std::string_view name);

        template<typename T>
        static ComponentTypeID GetID() {
            static_assert(std::is_same_v<typename T::ComponentSelf, T>, "T is missing VKE_COMPONENT(T)");
            VKE_CORE_ASSERT(T::s_ComponentTypeID != kInvalidComponentType,
                            "Component type was never registered; was RegisterEngineComponents called?");
            return T::s_ComponentTypeID;
        }

        // IDs go from 0 to GetCount() - 1, in the order the types were registered.
        static u32 GetCount();
        // Null if no type has that ID or name.
        static const ComponentTypeInfo* Find(ComponentTypeID id);
        static const ComponentTypeInfo* Find(std::string_view name);

    private:
        static ComponentTypeID Add(ComponentTypeInfo info);
    };

} // namespace VKE::Scene
