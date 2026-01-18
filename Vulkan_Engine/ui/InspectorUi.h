#pragma once

#include <cstring>
#include <filesystem>
#include <string>

#include "Entity.h"
#include "components/Transform.h"
#include "components/MeshComponent.h"
#include "imgui.h"
#include "managers/SceneManager.h"
#include "ui/AssetBrowser.h"

// What the inspector is currently inspecting.
enum class InspectorSelectionType {
  None,
  Entity,
  Asset
};

struct InspectorSelection {
  InspectorSelectionType type = InspectorSelectionType::None;
  int entityId = 0;  // valid only if type == Entity
  std::string assetPath;     // valid only if type == Asset
};

// When selecting an asset we may be "picking" something for the inspector
// rather than just inspecting the asset itself.
enum class InspectorPickTarget {
	None,
	MeshAlbedo
};

class InspectorUi {
 public:
  // Selection control API
  static void selectEntity(int entityId);
  static void selectAsset(const std::string& assetPath);
  static void clearSelection();

  static void render();

  // Query helpers
  static int getSelectedEntityId();  // returns -1 if no entity selected
  static const std::string& getSelectedAssetPath();

 private:
  static InspectorSelection selection;
  static InspectorPickTarget pickTarget;
};
