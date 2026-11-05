#pragma once

#include <assets/AssetTypes.h>

#include <filesystem>
#include <string>

namespace VKE::Editor {

// A copy on disk of a scene with unsaved changes, kept while its scripts run or a game DLL
// loads, in case that takes the editor down. Finding the file at start means it did.
class SceneRecovery {
public:
	void SetFile(std::filesystem::path file) { m_File = std::move(file); }

	bool Exists() const;
	void Write(const std::string &text, AssetID scene) const;
	// The scene's text and the asset it was opened from. False, logged, if the file can't be read.
	bool Read(std::string &text, AssetID &scene) const;
	void Discard() const;

private:
	std::filesystem::path m_File;
};

} // namespace VKE::Editor
