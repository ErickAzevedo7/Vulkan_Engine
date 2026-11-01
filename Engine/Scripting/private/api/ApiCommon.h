#pragma once

#include "ObjectHandles.h"
#include "ScriptRun.h"

#include <scene/GameObject.h>
#include <scene/Scene.h>

namespace VKE::Scripting {

    // The object behind `object`, or null if it is gone or no game is running.
    inline Scene::GameObject* FindObject(vke::Object object) {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetScene().Get(ToHandle(object)) : nullptr;
    }

    // The component of `T` on the object, or null.
    template<typename T>
    T* FindComponent(vke::Object object) {
        Scene::GameObject* found = FindObject(object);
        return found ? found->GetComponent<T>() : nullptr;
    }

} // namespace VKE::Scripting
