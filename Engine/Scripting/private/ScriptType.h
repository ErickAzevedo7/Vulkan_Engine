#pragma once

#include <scene/ComponentRegistry.h>
#include <scene/Properties.h>

#include <deque>
#include <string>
#include <vector>

namespace VKE::Scripting {

    // A script type of the game's DLL, in the terms the engine works with. The tables keep
    // pointing into the DLL, so this doesn't outlive it.
    struct ScriptType {
        std::string Name;
        Scene::ComponentTypeID ID = Scene::kInvalidComponentType;
        void* (*Create)() = nullptr;
        void (*Destroy)(void* script) = nullptr;
        Scene::StructInfo Properties = {nullptr, nullptr, 0};

        // What Properties points at.
        std::deque<std::vector<Scene::PropertyInfo>> PropertyLists;
        std::deque<std::vector<Scene::EnumValue>> EnumValues;
        std::deque<Scene::EnumInfo> Enums;
        std::deque<Scene::StructInfo> Structs;
    };

} // namespace VKE::Scripting
