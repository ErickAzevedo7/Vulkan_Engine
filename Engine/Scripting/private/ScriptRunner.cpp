#include "scripting/ScriptRunner.h"
#include "ScriptRun.h"

namespace VKE::Scripting {

    ScriptRunner::ScriptRunner() = default;

    ScriptRunner::~ScriptRunner() = default;

    void ScriptRunner::Start(Scene::Scene& scene) {
        m_Run = MakeIunique<ScriptRun>(scene);
    }

    void ScriptRunner::Update(f32 deltaTime) {
        if (m_Run) m_Run->Update(deltaTime);
    }

    void ScriptRunner::Stop() {
        m_Run.reset();
    }

} // namespace VKE::Scripting
