include_guard(GLOBAL)

# Locates glslc from the Vulkan SDK.
if(Vulkan_GLSLC_EXECUTABLE)
    set(VKE_GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
else()
    find_program(VKE_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" REQUIRED)
endif()

# vke_add_shaders(<target> SOURCES <files...> [INCLUDE_DIRS <dirs...>])
#
# Compiles each GLSL source to <name>.spv. Included files are tracked, so
# editing a shared include rebuilds every shader that uses it.
function(vke_add_shaders target)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDE_DIRS" ${ARGN})

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    set(depfile_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir")

    set(include_flags "")
    foreach(dir IN LISTS ARG_INCLUDE_DIRS)
        get_filename_component(dir_abs "${dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND include_flags "-I${dir_abs}")
    endforeach()

    set(outputs "")
    foreach(source IN LISTS ARG_SOURCES)
        get_filename_component(source_abs "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        get_filename_component(name "${source_abs}" NAME)
        set(output "${output_dir}/${name}.spv")
        set(depfile "${depfile_dir}/${name}.d")

        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}" "${depfile_dir}"
            COMMAND "${VKE_GLSLC}" --target-env=vulkan1.2 ${include_flags}
                    -MD -MF "${depfile}" -o "${output}" "${source_abs}"
            MAIN_DEPENDENCY "${source_abs}"
            DEPFILE "${depfile}"
            COMMENT "Compiling shader ${name}"
            VERBATIM
        )
        list(APPEND outputs "${output}")
    endforeach()

    add_custom_target(${target} DEPENDS ${outputs} SOURCES ${ARG_SOURCES})
    set_target_properties(${target} PROPERTIES
        VKE_SHADER_OUTPUT_DIR "${output_dir}"
        FOLDER "Shaders"
    )
endfunction()

# vke_stage_shaders(<executable> <shader targets...>)
#
# Copies compiled shaders to a "shaders" folder next to the executable.
function(vke_stage_shaders executable)
    set(commands "")
    foreach(shader_target IN LISTS ARGN)
        get_target_property(output_dir ${shader_target} VKE_SHADER_OUTPUT_DIR)
        list(APPEND commands
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${output_dir}" "$<TARGET_FILE_DIR:${executable}>/shaders"
        )
    endforeach()

    set(stage_target ${executable}StageShaders)
    add_custom_target(${stage_target} ${commands}
        COMMENT "Staging shaders for ${executable}"
        VERBATIM
    )
    add_dependencies(${stage_target} ${ARGN})
    add_dependencies(${executable} ${stage_target})
    set_target_properties(${stage_target} PROPERTIES FOLDER "Shaders")
endfunction()
