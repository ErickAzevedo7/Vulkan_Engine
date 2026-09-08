add_executable(Sandbox
    SandboxApp.cpp
)

target_link_libraries(Sandbox PRIVATE
    VKE_Engine
)

# Required by core/EntryPoint.h's platform guard.
# TODO(Phase 6): promote to the root CMakeLists.txt once Editor/Runtime exist.
target_compile_definitions(Sandbox PRIVATE VKE_PLATFORM_WINDOWS)

set_target_properties(Sandbox PROPERTIES FOLDER "Apps")
