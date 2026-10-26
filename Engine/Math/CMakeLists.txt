add_library(VKE_Math STATIC)

find_package(glm CONFIG REQUIRED)

target_sources(VKE_Math PRIVATE
    private/Transform.cpp
    private/AABB.cpp
)

target_include_directories(VKE_Math
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Math
    PUBLIC  glm::glm
            VKE_Reflection
)

set_target_properties(VKE_Math PROPERTIES FOLDER "Engine")
