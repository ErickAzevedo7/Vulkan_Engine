#pragma once
#include <vector>

#include "Scene.h"

class SceneManager {
 public:
  static void loadDefaults();

  // Creates a new scene and returns its index
  static size_t createScene(const std::string& name = "New Scene");

  // Removes a scene by index
  static void removeScene(size_t index);

  // Returns a pointer to the scene at the given index
  static Scene* getScene(size_t index);

  // Returns a pointer to the currently active scene
  static Scene* getActiveScene();

  // Sets the active scene by index
  static void setActiveScene(size_t index);

  // Returns the number of scenes managed
  static size_t getSceneCount();
  
private:
  static std::vector<std::unique_ptr<Scene>> scenes;
  static size_t activeSceneIndex;
};
