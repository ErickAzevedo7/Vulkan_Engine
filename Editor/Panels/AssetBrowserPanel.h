#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace VKE::Scene {
    class GameObject;
} // namespace VKE::Scene

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    class EditorScene;
    class MaterialEdits;

    // The asset roots' folders as a tree, and the files of the open folder as tiles or a list.
    // A click selects an asset for the Inspector and a double click opens a folder, a scene or
    // a prefab's opens it. A model's tile opens to show the meshes and materials inside it. Assets
    // are dragged onto asset fields, the Hierarchy and the Viewport, and files and folders onto
    // folders to move them; an object dragged from the Hierarchy becomes a prefab where it lands.
    // The context menu creates folders and materials, renames and deletes, always with the
    // .vkasset files, so assets keep their IDs. Read-only roots only show their files.
    class AssetBrowserPanel {
    public:
        AssetBrowserPanel(EditorScene& scene, MaterialEdits& edits, UI::UISystem& ui);

        // The window's title, with its icon.
        static const char* GetWindowName();

        void Draw(bool* open);

        // Lists the folders again, for files changed on disk.
        void Refresh() { ++m_Generation; }

        // The scene double-clicked since the last call, to open; invalid if none.
        Assets::AssetID TakeSceneToOpen();

    private:
        // A file, folder, or mesh or material inside a model, as the content shows it.
        struct Item {
            std::filesystem::path Path;
            std::string Name;
            bool IsFolder = false;
            Assets::AssetID ID;
            Assets::AssetType Type = Assets::AssetType::Model;
            // A mesh or material listed under its model's tile.
            bool IsSubAsset = false;
        };

        void DrawFolderTree();
        void DrawFolderRow(const std::filesystem::path& folder, const std::string& name, u32 depth, bool isRoot,
                           bool readOnly);
        void DrawToolbar();
        void DrawContent();
        void DrawTile(const Item& item, f32 width);
        void DrawListRow(const Item& item);
        // The click, drag, drop and menu of the item just drawn as an ImGui item; `released` is
        // whether a left click on it ended this frame.
        void HandleItem(const Item& item, bool released);
        void DrawItemMenu(const Item& item);
        void DrawEmptyMenu();
        void DrawDeleteDialog();
        void HandleShortcuts();

        // Takes files and folders dropped on the last item, to move them into `folder`, and
        // objects, to make prefabs there. Returns whether one is over it now.
        bool AcceptMove(const std::filesystem::path& folder);
        bool AcceptObject(const std::filesystem::path& folder);
        void Move(const std::filesystem::path& from, const std::filesystem::path& folder);
        void CreatePrefab(Scene::GameObject& object, const std::filesystem::path& folder);

        void UpdateListing();
        void CreateFolder();
        void CreateMaterial();
        void StartRename(const Item& item);
        void FinishRename();
        // The texture's image, loaded if it isn't and its tile is `visible`, one a frame; 0 until then.
        u64 GetThumbnail(Assets::AssetID texture, bool visible);

        bool IsReadOnly(const std::filesystem::path& path) const;
        bool IsSelected(const Item& item) const;

        EditorScene& m_Scene;
        MaterialEdits& m_Edits;
        UI::UISystem& m_UI;

        std::filesystem::path m_Folder;
        std::unordered_set<std::string> m_OpenFolders;
        std::unordered_set<Assets::AssetID> m_OpenModels;
        std::string m_Filter;
        bool m_ListView = false;
        f32 m_TreeWidth = 200.0f;
        bool m_Focused = false;

        // The content of m_Folder, made again when the folder, the filter or the generation changes.
        std::vector<Item> m_Items;
        std::filesystem::path m_ListedFolder;
        std::string m_ListedFilter;
        u64 m_Generation = 1;
        u64 m_ListedGeneration = 0;

        // The selected file or folder; assets are also selected in the scene, for the Inspector.
        std::filesystem::path m_SelectedPath;
        Assets::AssetID m_SelectedSubAsset;

        std::filesystem::path m_Renaming;
        std::string m_RenameText;
        bool m_FocusRename = false;

        std::filesystem::path m_ToDelete;
        bool m_OpenDeleteDialog = false;

        Assets::AssetID m_SceneToOpen;
        bool m_LoadedThumbnail = false;
    };

} // namespace VKE::Editor
