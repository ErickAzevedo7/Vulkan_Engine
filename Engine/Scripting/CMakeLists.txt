add_library(VKE_Scripting STATIC)

target_sources(VKE_Scripting PRIVATE
    private/GameModule.cpp
    private/ScriptComponent.cpp
    private/ScriptRun.cpp
    private/ScriptRunner.cpp
    private/api/ComponentApi.cpp
    private/api/GameApi.cpp
    private/api/LogApi.cpp
    private/api/ObjectApi.cpp
    private/api/TransformApi.cpp
)

target_include_directories(VKE_Scripting
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
            ${CMAKE_CURRENT_SOURCE_DIR}/private/api
)

target_link_libraries(VKE_Scripting
    PUBLIC  VKE_Core
            VKE_Assets
    PRIVATE VKE_Scene
            VKE_Platform
            VKE_SDK
)

set_target_properties(VKE_Scripting PROPERTIES FOLDER "Engine")
