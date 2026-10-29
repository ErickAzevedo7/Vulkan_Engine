add_library(VKE_Project STATIC)

find_package(yaml-cpp CONFIG REQUIRED)

target_sources(VKE_Project PRIVATE
    private/Project.cpp
)

target_include_directories(VKE_Project
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Project
    PUBLIC  VKE_Core
            VKE_Assets
    PRIVATE VKE_Platform
            yaml-cpp::yaml-cpp
)

set_target_properties(VKE_Project PROPERTIES FOLDER "Engine")
