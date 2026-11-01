add_executable(Sandbox
    SandboxApp.cpp
    SandboxSceneSystem.cpp
)

target_link_libraries(Sandbox PRIVATE
    VKE_Engine
)

# Required by core/EntryPoint.h's platform guard.
# TODO(Phase 6): promote to the root CMakeLists.txt once Editor/Runtime exist.
target_compile_definitions(Sandbox PRIVATE VKE_PLATFORM_WINDOWS)

# Started with --project <folder>; the test assets are outside the repo, in a folder that has a
# Sandbox.vkproject. VS Code passes it from .vscode/launch.json.
set_target_properties(Sandbox PROPERTIES FOLDER "Apps")

vke_stage_shaders(Sandbox RendererShaders)
vke_stage_engine_content(Sandbox)
