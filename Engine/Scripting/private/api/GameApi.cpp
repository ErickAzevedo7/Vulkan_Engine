#include "ApiCommon.h"

#include <platform/Input.h>

#include <vke/detail/ApiBindings.h>

namespace vke::api {

    namespace {
        using VKE::Platform::IInput;
        using VKE::Platform::KeyCode;
        using VKE::Scripting::ScriptRun;

        static_assert(static_cast<int>(Key::Count) == static_cast<int>(KeyCode::Count));
        static_assert(static_cast<int>(Key::Pad0) == static_cast<int>(KeyCode::KP0));
        static_assert(static_cast<int>(Key::Menu) == static_cast<int>(KeyCode::Menu));

        // Null while the keyboard and the mouse aren't the game's.
        const IInput* GetInput() {
            ScriptRun* run = ScriptRun::GetCurrent();
            return run && run->HasInputFocus() ? &run->GetInput() : nullptr;
        }

        KeyCode ToEngine(Key key) {
            return static_cast<KeyCode>(key);
        }

        VKE::Platform::MouseButton ToEngine(MouseButton button) {
            return static_cast<VKE::Platform::MouseButton>(button);
        }
    } // namespace

    bool Input_IsKeyDown(Key key) {
        const IInput* input = GetInput();
        return input && input->IsKeyDown(ToEngine(key));
    }

    bool Input_WasKeyPressed(Key key) {
        const IInput* input = GetInput();
        return input && input->IsKeyPressed(ToEngine(key));
    }

    bool Input_WasKeyReleased(Key key) {
        const IInput* input = GetInput();
        return input && input->IsKeyReleased(ToEngine(key));
    }

    bool Input_IsMouseDown(MouseButton button) {
        const IInput* input = GetInput();
        return input && input->IsMouseButtonDown(ToEngine(button));
    }

    bool Input_WasMousePressed(MouseButton button) {
        const IInput* input = GetInput();
        return input && input->IsMouseButtonPressed(ToEngine(button));
    }

    bool Input_WasMouseReleased(MouseButton button) {
        const IInput* input = GetInput();
        return input && input->IsMouseButtonReleased(ToEngine(button));
    }

    float Input_GetMouseX() {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetInput().GetMouseX() - run->GetViewX() : 0.0f;
    }

    float Input_GetMouseY() {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetInput().GetMouseY() - run->GetViewY() : 0.0f;
    }

    float Input_GetScroll() {
        const IInput* input = GetInput();
        return input ? input->GetScrollDeltaY() : 0.0f;
    }

    float Time_GetDeltaTime() {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetDeltaTime() : 0.0f;
    }

    float Time_GetTime() {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetTime() : 0.0f;
    }

    int Time_GetFrame() {
        ScriptRun* run = ScriptRun::GetCurrent();
        return run ? run->GetFrame() : 0;
    }

    bool Physics_Raycast(Vec3 origin, Vec3 direction, RaycastHit& hit, float maxDistance) {
        ScriptRun* run = ScriptRun::GetCurrent();
        VKE::Scene::RaycastHit found;
        if (!run || !run->GetScene().Raycast(VKE::Scripting::ToEngine(origin), VKE::Scripting::ToEngine(direction),
                                             maxDistance, false, found)) {
            return false;
        }
        hit.object = found.Object ? VKE::Scripting::ToObject(found.Object->GetHandle()) : Object();
        hit.point = VKE::Scripting::ToSDK(found.Point);
        hit.normal = VKE::Scripting::ToSDK(found.Normal);
        hit.distance = found.Distance;
        return true;
    }

    void LoadScene(SceneRef scene) {
        ScriptRun* run = ScriptRun::GetCurrent();
        if (run && scene.IsSet()) run->RequestScene({detail::Bits::GetID(scene)});
    }

    void Quit() {
        if (ScriptRun* run = ScriptRun::GetCurrent()) run->RequestQuit();
    }

} // namespace vke::api
