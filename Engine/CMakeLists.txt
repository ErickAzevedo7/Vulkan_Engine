project(VulkanEngine_Engine)

add_library(VKE_Engine INTERFACE)

# Phase 1 modules
add_subdirectory(reflection)
add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(events)

# Phase 2 modules
add_subdirectory(platform)
add_subdirectory(rhi)

# Phase 3 modules
add_subdirectory(assets)
add_subdirectory(renderer)

# Phase 4 modules
add_subdirectory(physics)
add_subdirectory(scene)

# Phase 6 modules
add_subdirectory(ui)
add_subdirectory(project)

# Phase 5 modules
add_subdirectory(sdk)
add_subdirectory(scripting)

# The parent aggregates all modules — modules do NOT link to VKE_Engine themselves
target_link_libraries(VKE_Engine INTERFACE
    VKE_Reflection
    VKE_Core
    VKE_Math
    VKE_Events
    VKE_Platform
    VKE_RHI
    VKE_Assets
    VKE_Renderer
    VKE_Physics
    VKE_Scene
    VKE_UI
    VKE_Project
    VKE_Scripting
)
