include_guard(GLOBAL)

set(VKE_CONTENT_DIR "${CMAKE_SOURCE_DIR}/Engine/Content")

# vke_stage_engine_content(<executable>)
#
# Copies the engine's content (shapes, default materials, fonts and their licenses) to a
# "Content" folder next to the executable, where the engine mounts it as a read-only asset root.
function(vke_stage_engine_content executable)
    add_custom_command(TARGET ${executable} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${VKE_CONTENT_DIR}" "$<TARGET_FILE_DIR:${executable}>/Content"
        COMMENT "Staging engine content for ${executable}"
        VERBATIM
    )
endfunction()
