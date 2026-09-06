add_library(VKE_Core STATIC)

find_package(spdlog CONFIG REQUIRED)

target_sources(VKE_Core PRIVATE
    private/Application.cpp
    private/Log.cpp
    private/ObjectID.cpp
)

target_include_directories(VKE_Core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Core
    PUBLIC  spdlog::spdlog
)

target_compile_definitions(VKE_Core PUBLIC
    $<$<CONFIG:Debug>:VKE_DEBUG VKE_ENABLE_ASSERTS>
)

set_target_properties(VKE_Core PROPERTIES FOLDER "Engine")
