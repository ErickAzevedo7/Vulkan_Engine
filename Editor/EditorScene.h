#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/ObjectID.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>
#include <scene/Prefab.h>
#include <scripting/ScriptRunner.h>

#include <filesystem>
#include <functional>
#include <string>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Platform {
    class IInput;
} // namespace VKE::Platform

namespace VKE::Scene {
    class GameObject;
    class Scene;
} // namespace VKE::Scene

namespace VKE::Editor {

    // The engine's shapes, in Content/Shapes.
    enum class Shape {
        Cube,
        Sphere,
        Plane,
        Cylinder,
    };

    // What the scene in the editor is doing.
    enum class PlayMode {
        Edit,
        Playing,
        // Playing, stopped in time.
        Paused,
    };

    // The scene open in the editor: its objects, the scene asset it was opened from and is
    // saved to, whether it changed since, and what is selected: an object or an asset.
    //
    // Play keeps the scene as text in memory and runs it with its scripts; Stop puts that text
    // back, so everything that happened in between is lost.
    //
    // A prefab can be opened in place of the scene, which waits in memory: the objects are then
    // the prefab's, and Save writes the prefab and remakes the scene's instances from it.
    class EditorScene {
    public:
        explicit EditorScene(Assets::AssetManager& assets);
        ~EditorScene();

        // An unsaved scene with a camera and a light.
        void New();
        // False, logged, if the asset isn't a scene or can't be read; the open scene stays.
        bool Open(Assets::AssetID scene);
        // Writes the scene to its file. False if it has none yet or the write fails.
        bool Save();
        // Writes the scene to `file`, which must be inside the project's assets, and makes it
        // the scene's file. False, logged, if it isn't or the write fails.
        bool SaveAs(const std::filesystem::path& file);

        Scene::Scene& GetScene() { return *m_Scene; }
        const Scene::Scene& GetScene() const { return *m_Scene; }
        Assets::AssetManager& GetAssets() { return m_Assets; }
        Scene::PrefabLibrary& GetPrefabs() { return m_Prefabs; }

        // Invalid until the scene is saved. While a prefab is open, these are the prefab's.
        Assets::AssetID GetID() const { return m_ID; }
        // Where the scene asset is now, which follows it when it is moved; empty until saved, or
        // once its file is deleted.
        std::filesystem::path GetFile() const;
        // "Sponza.scene", or "Untitled" until saved.
        std::string GetFileName() const;

        // While playing, whether it had changed when Play was pressed: nothing done since counts.
        bool IsDirty() const { return IsPlaying() ? m_DirtyAtPlay : m_Dirty; }
        void MarkDirty() { m_Dirty = true; }

        PlayMode GetPlayMode() const { return m_PlayMode; }
        // True while playing or paused.
        bool IsPlaying() const { return m_PlayMode != PlayMode::Edit; }
        // False, logged, if the scene can't be kept.
        bool Play();
        // Between playing and paused.
        void TogglePause();
        // Back to the scene as it was at Play, with the same object selected if it was there.
        void Stop();
        // What scripts read the keyboard and the mouse from.
        void SetInput(const Platform::IInput& input) { m_Input = &input; }
        // Whether they are the game's now, and where its view starts in the window.
        void SetInputArea(bool focused, f32 viewX, f32 viewY) { m_Runner.SetInputArea(focused, viewX, viewY); }
        // Runs the frame's update for the mode, `deltaTime` seconds after the last. Call it once
        // per frame, before rendering.
        void Update(f32 deltaTime);

        // Changes when another scene is opened, but not when Stop restores this one.
        u32 GetOpenCount() const { return m_OpenCount; }

        bool IsEditingPrefab() const { return m_Waiting.Objects != nullptr; }
        // The name of the scene waiting while a prefab is open.
        std::string GetWaitingFileName() const;
        // False, logged, if the asset isn't a prefab or can't be read; the scene stays.
        bool OpenPrefab(Assets::AssetID prefab);
        // Back to the scene, dropping what wasn't saved of the prefab.
        void ClosePrefab();

        // Asks for a prefab to be opened, for what can't replace the scene while drawing it.
        void AskToOpenPrefab(Assets::AssetID prefab) { m_PrefabToOpen = prefab; }
        // The prefab asked for since the last call; invalid if none.
        Assets::AssetID TakePrefabToOpen();

        // Whether `object` could become a prefab: prefabs don't go inside prefabs.
        bool CanCreatePrefab(const Scene::GameObject& object) const;
        // Writes `object` and everything below it to `file`, and puts an instance of the new
        // prefab in their place. Invalid, logged, if it can't.
        Assets::AssetID CreatePrefab(Scene::GameObject& object, const std::filesystem::path& file);
        // Has `change` edit the prefab's objects, given by their root, writes the file and remakes
        // the scene's instances, which keep their overrides. Replaces every object of the scene.
        // False, logged, if the prefab can't be written.
        bool ChangePrefab(Assets::AssetID prefab, const std::function<void(Scene::GameObject&)>& change);
        // Makes an instance plain objects, which stay as they are, and marks the scene changed.
        void Unpack(Scene::GameObject& instance);

        // The hierarchy's rules. Prefab instances keep the shape of their prefab: only a whole
        // instance moves or goes away, and nothing goes inside one. A prefab has one root.
        // Whether `object` has to stay where it is and can't be destroyed.
        bool IsFixed(const Scene::GameObject& object) const;
        // Whether objects can go under `parent`, or at the root of the scene for null.
        bool AcceptsChildren(const Scene::GameObject* parent) const;
        // Where new objects go when no parent is given: the open prefab's root, or the scene's root.
        Scene::GameObject* GetDefaultParent() const;

        // The selected object, or null. Nothing is selected after New and Open.
        Scene::GameObject* GetSelected() const;
        // Selecting anything, or nothing, deselects the asset.
        void Select(Scene::GameObject* object);
        // The selected asset, or invalid. Selecting one deselects the object.
        Assets::AssetID GetSelectedAsset() const { return m_SelectedAsset; }
        void SelectAsset(Assets::AssetID asset);

        // New objects go last under `parent`, or at the root of the scene, and mark it changed.
        Scene::GameObject& CreateEmpty(Scene::GameObject* parent = nullptr);
        // Null, logged, if the engine's content is missing the shape.
        Scene::GameObject* CreateShape(Shape shape, Scene::GameObject* parent = nullptr);
        Scene::GameObject& CreateCamera(Scene::GameObject* parent = nullptr);
        Scene::GameObject& CreateLight(Renderer::LightType type, Scene::GameObject* parent = nullptr);
        // The objects of a model asset, last under `parent` or at the root of the scene, with the
        // root at `position` in its parent's space. Null, logged, if it isn't a model or can't load.
        Scene::GameObject* InstantiateModel(Assets::AssetID model, Scene::GameObject* parent = nullptr,
                                            const Vec3& position = Vec3(0.0f));
        // Whether the asset can go in the scene with Place: a model, or a prefab outside a prefab.
        bool CanPlace(Assets::AssetID asset) const;
        // The objects of a model or a new instance of a prefab, as InstantiateModel does.
        Scene::GameObject* Place(Assets::AssetID asset, Scene::GameObject* parent = nullptr,
                                 const Vec3& position = Vec3(0.0f));
        // Destroys the object and its children, and marks the scene changed.
        void Destroy(Scene::GameObject& object);

    private:
        // The scene while a prefab is open in its place.
        struct WaitingScene {
            Iunique<Scene::Scene> Objects;
            Assets::AssetID ID;
            bool Dirty = false;
            Handle<Scene::GameObject> Selected;
        };

        // Replaces `scene` with the one `text` holds, with the same object selected if it's there.
        bool Reload(Iunique<Scene::Scene>& scene, Handle<Scene::GameObject>& selected, const std::string& text,
                    const std::string& name);
        bool SavePrefab();
        Scripting::GameContext MakeGameContext();
        // A script asked for another scene. Stop still goes back to the one Play started from.
        void SwitchPlayedScene(Assets::AssetID scene);

        Assets::AssetManager& m_Assets;
        Scene::PrefabLibrary m_Prefabs;
        Iunique<Scene::Scene> m_Scene;
        Assets::AssetID m_ID;
        u32 m_OpenCount = 0;
        bool m_Dirty = false;
        PlayMode m_PlayMode = PlayMode::Edit;
        std::string m_PlaySnapshot;
        bool m_DirtyAtPlay = false;
        Handle<Scene::GameObject> m_Selected;
        Assets::AssetID m_SelectedAsset;
        WaitingScene m_Waiting;
        Assets::AssetID m_PrefabToOpen;
        const Platform::IInput* m_Input = nullptr;
        // Last: it stops the scripts of a scene that is still there.
        Scripting::ScriptRunner m_Runner;
    };

} // namespace VKE::Editor
