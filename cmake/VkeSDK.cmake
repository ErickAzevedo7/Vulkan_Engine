include_guard(GLOBAL)

# vke_stage_sdk(<executable>)
#
# Copies what a game is built with to an "SDK" folder next to the executable: the SDK's headers,
# the generated table, the tool that reads the marks and the CMake file projects include.
function(vke_stage_sdk executable)
    set(folder "$<TARGET_FILE_DIR:${executable}>/SDK")
    add_dependencies(${executable} VKE_Reflect)
    add_custom_command(TARGET ${executable} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${VKE_SDK_SOURCE_DIR}/public" "${folder}/include"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${VKE_SDK_GENERATED_DIR}" "${folder}/include"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:VKE_Reflect>" "${folder}/bin/VKE_Reflect.exe"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${VKE_SDK_SOURCE_DIR}/VkeGame.cmake" "${folder}/VkeGame.cmake"
        COMMENT "Staging the SDK for ${executable}"
        VERBATIM
    )
endfunction()
