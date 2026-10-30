#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/ObjectID.h>
#include <core/Types.h>

#include <string>
#include <unordered_set>

namespace VKE::Scene {
    class GameObject;
} // namespace VKE::Scene

namespace VKE::Editor {

    class EditorScene;

    // The scene's objects as a tree. A click selects; a double click or F2 renames; dragging a
    // row onto another makes it a child, and between two rows moves it there. A model dragged
    // from Assets onto a row goes inside it, and under the rows to the root. The context menu
    // creates, renames and destroys objects, and the eye on a row turns the object on and off.
    // With a filter, the objects whose names match are listed flat.
    class HierarchyPanel {
    public:
        explicit HierarchyPanel(EditorScene& scene);

        // The window's title, with its icon.
        static const char* GetWindowName();

        void Draw(bool* open);

    private:
        // Where a dragged row goes, next to the row it is dropped on.
        enum class DropPlace {
            Before,
            Inside,
            After,
        };

        void DrawToolbar();
        void DrawTree();
        void DrawRow(Scene::GameObject& object, u32 depth, bool canDrag);
        void DrawRowMenu(Scene::GameObject& object);
        void HandleShortcuts();

        // Moves `dragged` next to or inside `target`; a null target makes it the last root.
        void Move(Scene::GameObject& dragged, Scene::GameObject* target, DropPlace place);
        void StartRename(Scene::GameObject& object);
        // Opens the parents of the object, so its row shows.
        void Reveal(const Scene::GameObject& object);

        EditorScene& m_Scene;
        std::string m_Filter;
        std::unordered_set<ObjectID> m_Open;
        bool m_Focused = false;

        // The row being renamed and its text.
        Handle<Scene::GameObject> m_Renaming;
        std::string m_RenameText;
        bool m_FocusRename = false;

        // The selection the panel last saw: when something else changes it, the row is revealed.
        Handle<Scene::GameObject> m_LastSelected;
        bool m_ScrollToSelected = false;

        // Changes to the hierarchy wait until the tree is drawn, which walks it.
        Handle<Scene::GameObject> m_Dragged;
        Handle<Scene::GameObject> m_DropTarget;
        DropPlace m_DropPlace = DropPlace::Inside;
        bool m_DropPending = false;
        // A model dropped from Assets, placed under m_ModelParent, or at the root if it is invalid.
        Assets::AssetID m_ModelToPlace;
        Handle<Scene::GameObject> m_ModelParent;
        Handle<Scene::GameObject> m_ToDestroy;
    };

} // namespace VKE::Editor
