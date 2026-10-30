add_executable(Editor
    EditorApp.cpp
    EditorSystem.cpp
    EditorScene.cpp
    Display.cpp
    Fields.cpp
    LogCapture.cpp
    ObjectMenus.cpp
    ProjectHub.cpp
    ProjectTemplates.cpp
    RecentProjects.cpp
    Widgets.cpp
    Panels/ConsolePanel.cpp
    Panels/HierarchyPanel.cpp
    Panels/InspectorPanel.cpp
    Panels/ViewportPanel.cpp
    Gizmo/GizmoMath.cpp
    Gizmo/GizmoLayout.cpp
    Gizmo/TransformGizmo.cpp
    Gizmo/GizmoDrawing.cpp
    Viewport/EditorCamera.cpp
    Viewport/SceneMarkers.cpp
    Viewport/WorldShapes.cpp
)

find_package(yaml-cpp CONFIG REQUIRED)

target_include_directories(Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Editor PRIVATE
    VKE_Engine
    yaml-cpp::yaml-cpp
)

# Required by core/EntryPoint.h's platform guard.
target_compile_definitions(Editor PRIVATE VKE_PLATFORM_WINDOWS)

set_target_properties(Editor PROPERTIES FOLDER "Apps")

vke_stage_shaders(Editor RendererShaders)
vke_stage_engine_content(Editor)

# Project templates go to a "Templates" folder next to the editor.
add_custom_command(TARGET Editor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Templates" "$<TARGET_FILE_DIR:Editor>/Templates"
    COMMENT "Staging project templates for Editor"
    VERBATIM
)
