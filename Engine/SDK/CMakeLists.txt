# What scripts see. Includes nothing of the engine: a game's DLL is built against this alone.
add_library(VKE_SDK INTERFACE)

set(headers
    public/vke/Log.h
    public/vke/Marks.h
    public/vke/Math.h
    public/vke/Object.h
    public/vke/Script.h
    public/vke/detail/GameInfo.h
    public/vke/detail/ScriptCalls.h
)

set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(detail "${generated}/vke/detail")
add_custom_command(
    OUTPUT "${detail}/ApiTable.h" "${detail}/ApiCalls.inl" "${detail}/ApiBindings.h"
    COMMAND VKE_Reflect --api --output "${detail}" ${headers}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS VKE_Reflect ${headers}
    COMMENT "Generating the SDK's table"
    VERBATIM
)
add_custom_target(VKE_SDK_Table DEPENDS "${detail}/ApiTable.h" "${detail}/ApiCalls.inl" "${detail}/ApiBindings.h")
set_target_properties(VKE_SDK_Table PROPERTIES FOLDER "Engine")
add_dependencies(VKE_SDK VKE_SDK_Table)

target_include_directories(VKE_SDK INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/public"
    "${generated}"
)

set(VKE_SDK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
set(VKE_SDK_GENERATED_DIR "${generated}" CACHE INTERNAL "")
