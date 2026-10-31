#pragma once

#include <assets/AssetTypes.h>
#include <core/ObjectID.h>
#include <core/Types.h>

#include <filesystem>
#include <unordered_map>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {

    class Component;
    class GameObject;
    class Scene;
    struct PropertyInfo;

    // Prefabs: .prefab files holding one object with everything below it, in the format of a
    // scene's objects, and their instances in scenes.
    //
    // An instance is a copy of the prefab's objects (its parts) linked to the prefab. Its root
    // has its own ID; the others get IDs that follow from the root's and the part's, so they
    // stay the same every time the scene is loaded. What differs from the prefab is an override,
    // found by comparing the copy with the prefab, so a value that matches again stops being one.
    // The position and rotation of an instance's root say where the instance is, and its name
    // what it is called: they are saved, but they aren't overrides.
    //
    // The library keeps each prefab read from its file as objects of a scene of their own, never
    // updated or shown, which is what instances are copied from and compared with.
    class PrefabLibrary {
    public:
        explicit PrefabLibrary(Assets::AssetManager& assets);
        ~PrefabLibrary();

        PrefabLibrary(const PrefabLibrary&) = delete;
        PrefabLibrary& operator=(const PrefabLibrary&) = delete;

        Assets::AssetManager& GetAssets() const { return m_Assets; }

        // The prefab's root, read from its file the first time. Null if the asset isn't a prefab
        // or its file can't be read, which is logged once. Changing what it returns changes what
        // new instances copy; Save writes it.
        GameObject* GetRoot(Assets::AssetID prefab);
        // What `object` copies, or null if it isn't an instance's part or the prefab lost it.
        GameObject* GetPart(const GameObject& object);
        // The same for a component of an instance.
        Component* GetPart(const Component& component);
        // The prefab is read from its file again the next time it is needed.
        void Forget(Assets::AssetID prefab);

        // Writes `root` and everything below it to `file`, which becomes a prefab with those
        // objects' IDs as its parts'. `root` stays as it is. Invalid, logged, if an object is
        // already part of an instance, as prefabs don't go inside prefabs, or the write fails.
        Assets::AssetID Create(GameObject& root, const std::filesystem::path& file);
        // Writes the prefab's objects, as GetRoot gives them, to its file.
        bool Save(Assets::AssetID prefab);

        // A new instance last under `parent`, or at the root of the scene, with `id` as its
        // root's ID. Null if the prefab can't be read.
        GameObject* Instantiate(Scene& scene, Assets::AssetID prefab, GameObject* parent, ObjectID id);

        // Whether `object` or anything below it is part of an instance.
        static bool HasParts(const GameObject& object);
        // The part of `root`'s instance that copies `part`, or null.
        static GameObject* FindInstancePart(GameObject& root, ObjectID part);
        // A reference held by something on `owner` keeps meaning the same object when the prefab
        // is copied: inside an instance it is the ID of the part in the prefab, so it counts as
        // equal to the prefab's value and each instance reaches its own part. These go from an
        // object to its reference and back; the object found can be null.
        static ObjectID GetReference(GameObject& owner, GameObject& target);
        static GameObject* FindReferenced(GameObject& owner, ObjectID reference);
        // Makes the instance of `root` plain objects, as they are now.
        static void Unpack(GameObject& root);

        // Whether this property only says where an instance is, and isn't an override.
        static bool IsPlacement(const Component& component, const PropertyInfo& property);

        // Over the instance of `root`: how many overrides it has, dropping them, and writing them
        // into the prefab's objects, which Save then writes to the file.
        u32 CountOverrides(GameObject& root);
        void RevertOverrides(GameObject& root);
        void ApplyOverrides(GameObject& root);

    private:
        Assets::AssetManager& m_Assets;
        // Null for a prefab that couldn't be read.
        std::unordered_map<Assets::AssetID, Iunique<Scene>> m_Prefabs;
    };

} // namespace VKE::Scene
