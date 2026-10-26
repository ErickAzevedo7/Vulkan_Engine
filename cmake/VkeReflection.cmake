include_guard(GLOBAL)

# vke_generate_reflection(<target> FUNCTION <name> HEADERS <files...>)
#
# Runs VKE_Reflect over the headers and compiles the property tables it writes into <target>.
# The tables are written again whenever a header or the generator changes. The generated file
# defines ComponentRegistry::<name>, a static member declared in scene/ComponentRegistry.h, which
# registers the components found in the headers.
function(vke_generate_reflection target)
    cmake_parse_arguments(ARG "" "FUNCTION" "HEADERS" ${ARGN})

    set(headers "")
    foreach(header IN LISTS ARG_HEADERS)
        get_filename_component(header_abs "${header}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND headers "${header_abs}")
    endforeach()

    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}.reflection.cpp")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND VKE_Reflect --output "${output}" --function "${ARG_FUNCTION}" ${headers}
        DEPENDS VKE_Reflect ${headers}
        COMMENT "Generating property tables for ${target}"
        VERBATIM
    )
    target_sources(${target} PRIVATE "${output}")
endfunction()
