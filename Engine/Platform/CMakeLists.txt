add_library(VKE_Platform STATIC)

find_package(glfw3 CONFIG REQUIRED)

target_sources(VKE_Platform PRIVATE
    private/FileSystem.cpp
    private/Timer.cpp
    private/FileDialog.cpp
    private/PlatformGlobals.cpp
    private/PlatformSystem.cpp
    private/glfw/GLFWWindow.cpp
    private/glfw/GLFWInput.cpp
)

target_include_directories(VKE_Platform
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Platform
    PUBLIC  VKE_Core
    PRIVATE glfw
)

set_target_properties(VKE_Platform PROPERTIES FOLDER "Engine")
