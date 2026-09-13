add_executable(Sandbox
    SandboxApp.cpp
    TriangleRenderSystem.cpp
)

target_link_libraries(Sandbox PRIVATE
    VKE_Engine
)

# Required by core/EntryPoint.h's platform guard.
# TODO(Phase 6): promote to the root CMakeLists.txt once Editor/Runtime exist.
target_compile_definitions(Sandbox PRIVATE VKE_PLATFORM_WINDOWS)

set_target_properties(Sandbox PROPERTIES FOLDER "Apps")

# --- Shader compilation ---
# No shader build tool exists yet — this is a minimal, standalone step:
# compile the two triangle-test shaders to SPIR-V next to Sandbox.exe via
# glslc, located through VULKAN_SDK (already required by the root
# CMakeLists.txt).
if(DEFINED ENV{VULKAN_SDK})
    set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
else()
    find_program(GLSLC glslc)
endif()

set(SANDBOX_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
)
set(SANDBOX_SHADER_OUTPUTS "")

foreach(SHADER_SOURCE ${SANDBOX_SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
    # CMAKE_CURRENT_BINARY_DIR (not $<TARGET_FILE_DIR:Sandbox> — that
    # generator expression isn't resolvable in add_custom_command's OUTPUT)
    # matches where Sandbox.exe itself lands under this single-config Ninja
    # generator.
    set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${GLSLC}" ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SANDBOX_SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(SandboxShaders DEPENDS ${SANDBOX_SHADER_OUTPUTS})
add_dependencies(Sandbox SandboxShaders)
