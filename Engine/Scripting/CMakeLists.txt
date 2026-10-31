add_library(VKE_Scripting STATIC)

target_sources(VKE_Scripting PRIVATE
    private/GameModule.cpp
    private/api/LogApi.cpp
)

target_include_directories(VKE_Scripting
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Scripting
    PUBLIC  VKE_Core
    PRIVATE VKE_Scene
            VKE_Platform
            VKE_SDK
)

set_target_properties(VKE_Scripting PROPERTIES FOLDER "Engine")
