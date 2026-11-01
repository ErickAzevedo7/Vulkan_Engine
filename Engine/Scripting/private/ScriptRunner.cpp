#include "scripting/ScriptRunner.h"
#include "ScriptRun.h"

namespace VKE::Scripting {

    ScriptRunner::ScriptRunner() = default;

    ScriptRunner::~ScriptRunner() = default;

    void ScriptRunner::Start(Scene::Scene& scene, const GameContext& context) {
        m_Run.reset();
        m_Run = MakeIunique<ScriptRun>(scene, context);
        m_Run->SetInputArea(m_InputFocused, m_ViewX, m_ViewY);
    }

    void ScriptRunner::Update(f32 deltaTime) {
        if (!m_Run) return;
        m_Run->Update(deltaTime);

        // Copies: these can end the run, which owns the originals.
        if (m_Run->IsQuitRequested()) {
            auto quit = m_Run->GetContext().Quit;
            quit();
        } else if (Assets::AssetID scene = m_Run->GetRequestedScene(); scene.IsValid()) {
            auto load = m_Run->GetContext().LoadScene;
            load(scene);
        }
    }

    void ScriptRunner::Stop() {
        m_Run.reset();
    }

    void ScriptRunner::SetInputArea(bool focused, f32 viewX, f32 viewY) {
        m_InputFocused = focused;
        m_ViewX = viewX;
        m_ViewY = viewY;
        if (m_Run) m_Run->SetInputArea(focused, viewX, viewY);
    }

} // namespace VKE::Scripting
