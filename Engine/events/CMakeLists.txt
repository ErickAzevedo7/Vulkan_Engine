add_library(VKE_Events STATIC)

target_sources(VKE_Events PRIVATE
    private/EventBus.cpp
)

target_include_directories(VKE_Events
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Events
    PUBLIC  VKE_Core
            VKE_Math
)

set_target_properties(VKE_Events PROPERTIES FOLDER "Engine")
