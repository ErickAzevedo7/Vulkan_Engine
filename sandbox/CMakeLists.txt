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

target_compile_definitions(Sandbox PRIVATE
    VKE_SANDBOX_ASSET_ROOT="${CMAKE_SOURCE_DIR}/Vulkan_Engine/projects/assets"
)

set_target_properties(Sandbox PROPERTIES FOLDER "Apps")

vke_stage_shaders(Sandbox RendererShaders)
vke_stage_ui_fonts(Sandbox)
