#pragma once
#include "imgui.h"
#include "managers/SceneManager.h"
#include "components/Transform.h"

class SceneUi {
 public:
  static void render();
private:
  static int selectedEntity;
};
