add_library(VKE_Platform STATIC)

find_package(glfw3 CONFIG REQUIRED)

target_sources(VKE_Platform PRIVATE
    private/FileSystem.cpp
    private/Timer.cpp
    private/PlatformGlobals.cpp
    private/PlatformSystem.cpp
    private/glfw/GLFWWindow.cpp
    private/glfw/GLFWInput.cpp
)

# Native file dialogs and processes where the system has them; the others get stubs that log a warning.
if(WIN32)
    target_sources(VKE_Platform PRIVATE
        private/windows/WindowsFileDialog.cpp
        private/windows/WindowsProcess.cpp
        private/windows/WindowsDynamicLibrary.cpp
        private/windows/WindowsGuardedCall.cpp
    )
    target_link_libraries(VKE_Platform PRIVATE ole32 shell32)
else()
    target_sources(VKE_Platform PRIVATE
        private/FileDialog.cpp
        private/Process.cpp
        private/DynamicLibrary.cpp
        private/GuardedCall.cpp
    )
endif()

target_include_directories(VKE_Platform
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Platform
    PUBLIC  VKE_Core
    PRIVATE glfw
)

set_target_properties(VKE_Platform PROPERTIES FOLDER "Engine")
