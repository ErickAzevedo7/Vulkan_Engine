project(VulkanEngine_Engine)

add_library(VKE_Engine INTERFACE)

# Phase 1 modules
add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(events)

# Phase 2 modules
add_subdirectory(platform)
add_subdirectory(rhi)

# Phase 3 modules
add_subdirectory(assets)
add_subdirectory(renderer)

# The parent aggregates all modules — modules do NOT link to VKE_Engine themselves
target_link_libraries(VKE_Engine INTERFACE
    VKE_Core
    VKE_Math
    VKE_Events
    VKE_Platform
    VKE_RHI
    VKE_Assets
    VKE_Renderer
)

# Later modules — uncomment as implemented
# add_subdirectory(scene)
# add_subdirectory(physics)
# add_subdirectory(scripting)
# add_subdirectory(ui)
