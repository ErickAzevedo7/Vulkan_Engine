add_library(VKE_Events INTERFACE)

target_include_directories(VKE_Events
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(VKE_Events
    INTERFACE VKE_Core
              VKE_Math
)

set_target_properties(VKE_Events PROPERTIES FOLDER "Engine")
