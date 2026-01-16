#pragma once
#include "imgui.h"
#include "managers/SceneManager.h"
#include "components/Transform.h"
#include "ui/AssetBrowser.h"

class SceneUi {
 public:
  static void render();

  static int selectedEntity;

 private:
};
