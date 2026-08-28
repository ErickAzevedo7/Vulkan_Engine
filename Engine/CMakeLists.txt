project(VulkanEngine_Engine)

add_library(VKE_Engine INTERFACE)

# Phase 1 modules
add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(events)

# The parent aggregates all modules — modules do NOT link to VKE_Engine themselves
target_link_libraries(VKE_Engine INTERFACE
    VKE_Core
    VKE_Math
    VKE_Events
)

# Phase 2+ modules — uncomment as implemented
# add_subdirectory(platform)
# add_subdirectory(rhi)
# add_subdirectory(renderer)
# add_subdirectory(scene)
# add_subdirectory(physics)
# add_subdirectory(assets)
# add_subdirectory(scripting)
# add_subdirectory(ui)
