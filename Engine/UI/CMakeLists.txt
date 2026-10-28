add_library(VKE_UI STATIC)

find_package(imgui CONFIG REQUIRED)

target_sources(VKE_UI PRIVATE
    private/UISystem.cpp
    private/Theme.cpp
    private/FontFile.cpp
)

target_include_directories(VKE_UI
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# ImGui is part of this module's interface: the editor calls it directly.
target_link_libraries(VKE_UI
    PUBLIC  VKE_Core
            VKE_Math
            VKE_Renderer
            imgui::imgui
    PRIVATE VKE_Platform
)

set_target_properties(VKE_UI PROPERTIES FOLDER "Engine")

set(VKE_UI_FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts" CACHE INTERNAL "")

# vke_stage_ui_fonts(<executable>)
#
# Copies the UI's fonts and their licenses to a "fonts" folder next to the executable.
function(vke_stage_ui_fonts executable)
    add_custom_command(TARGET ${executable} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${VKE_UI_FONT_DIR}" "$<TARGET_FILE_DIR:${executable}>/fonts"
        COMMENT "Staging UI fonts for ${executable}"
        VERBATIM
    )
endfunction()
