add_executable(Runtime
    RuntimeApp.cpp
)

target_link_libraries(Runtime PRIVATE
    VKE_Engine
)

# Required by core/EntryPoint.h's platform guard.
target_compile_definitions(Runtime PRIVATE VKE_PLATFORM_WINDOWS)

set_target_properties(Runtime PROPERTIES FOLDER "Apps")

vke_stage_shaders(Runtime RendererShaders)
vke_stage_engine_content(Runtime)
