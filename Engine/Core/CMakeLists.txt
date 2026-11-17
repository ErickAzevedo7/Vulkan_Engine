add_library(VKE_Core STATIC)

find_package(spdlog CONFIG REQUIRED)

target_sources(VKE_Core PRIVATE
    private/Application.cpp
    private/Log.cpp
    private/ObjectID.cpp
    private/IDUtils.cpp
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

# Tracy's client, for CPU zones (core/Profiling.h). It records only while the Tracy viewer
# is connected, and is built without its crash handler, which would catch a script's crash
# before the engine's own handler can stop Play.
option(VKE_ENABLE_TRACY "Build the engine with Tracy's profiler client" ON)
if(VKE_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(VKE_Core PUBLIC Tracy::TracyClient)
    target_compile_definitions(VKE_Core PUBLIC VKE_PROFILING)
endif()

set_target_properties(VKE_Core PROPERTIES FOLDER "Engine")
