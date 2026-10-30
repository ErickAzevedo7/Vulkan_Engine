#pragma once

#include <assets/AssetTypes.h>

#include <unordered_set>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Editor {

    // The .mat materials changed in the editor and not saved yet. A change goes to the loaded
    // material at once, so every scene using it shows it; saving writes the file and reverting
    // reads it again.
    class MaterialEdits {
    public:
        explicit MaterialEdits(Assets::AssetManager& assets);

        // Replaces the values of the material loaded from `material`. False for materials that
        // aren't writable .mat files.
        bool Change(Assets::AssetID material, Assets::MaterialData values);

        bool IsUnsaved(Assets::AssetID material) const { return m_Unsaved.count(material) != 0; }
        size_t GetUnsavedCount() const { return m_Unsaved.size(); }

        bool Save(Assets::AssetID material);
        // False if any of them wasn't saved; those stay unsaved.
        bool SaveAll();
        void Revert(Assets::AssetID material);

        // Forgets materials whose files are gone.
        void DropMissing();

    private:
        Assets::AssetManager& m_Assets;
        std::unordered_set<Assets::AssetID> m_Unsaved;
    };

} // namespace VKE::Editor
