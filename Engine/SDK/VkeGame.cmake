# Builds a game's scripts. Set VKE_SDK_DIR to the SDK folder next to the editor.
include_guard(GLOBAL)

if(NOT VKE_SDK_DIR)
    message(FATAL_ERROR "VKE_SDK_DIR isn't set: pass -DVKE_SDK_DIR=<the SDK folder next to the editor>")
endif()

# vke_add_game(<target>)
#
# Makes a DLL of every .cpp under the current folder, next to the tables of the scripts in the .h
# files, and puts it in the project's .vke/bin folder, where the engine looks for it.
function(vke_add_game target)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

    set(tables "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}.tables.cpp")
    add_custom_command(
        OUTPUT "${tables}"
        COMMAND "${VKE_SDK_DIR}/bin/VKE_Reflect" --game --root "${CMAKE_CURRENT_SOURCE_DIR}" --output "${tables}" ${headers}
        DEPENDS ${headers}
        COMMENT "Reading the scripts' marks"
        VERBATIM
    )

    add_library(${target} SHARED ${sources} "${tables}")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${VKE_SDK_DIR}/include")
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "Game"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.vke/bin"
    )
endfunction()
